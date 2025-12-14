// kernel/virtio_disk.c
#include "riscv.h"
#include "fs.h"
#include "buf.h"
#include "memory.h"
#include "spinlock.h"
#include "string.h"
#include "console.h"
#include "proc.h"

// virtio mmio 接口定义
#define VIRTIO0 0x10001000
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE     0x028
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
//#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070
#define VIRTIO_MMIO_QUEUE_PFN           0x040 

#define VIRTIO_CONFIG_S_ACKNOWLEDGE     1
#define VIRTIO_CONFIG_S_DRIVER          2
#define VIRTIO_CONFIG_S_DRIVER_OK       4
#define VIRTIO_CONFIG_S_FEATURES_OK     8

#define VIRTIO_BLK_F_RO                 5
#define VIRTIO_BLK_F_SCSI               7
#define VIRTIO_BLK_F_CONFIG_WCE         11
#define VIRTIO_BLK_F_MQ                 12
#define VIRTIO_F_ANY_LAYOUT             27
#define VIRTIO_RING_F_INDIRECT_DESC     28
#define VIRTIO_RING_F_EVENT_IDX         29

#define VIRTIO_BLK_T_IN                 0
#define VIRTIO_BLK_T_OUT                1

#define NUM 8 

// 前向声明
void virtio_disk_intr(void);

struct virtq_desc {
  uint64 addr;
  uint32 len;
  uint16 flags;
  uint16 next;
};

struct virtq_avail {
  uint16 flags;
  uint16 idx;
  uint16 ring[NUM];
};

struct virtq_used_elem {
  uint32 id;
  uint32 len;
};

struct virtq_used {
  uint16 flags;
  uint16 idx;
  struct virtq_used_elem ring[NUM];
};

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

struct {
  // 【关键修复】强制 4096 字节对齐，否则 QEMU 无法正确寻址
  char pages[2*PGSIZE] __attribute__ ((aligned (4096))); 
  
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;

  char free[NUM];  
  uint16 used_idx; 

  struct {
    struct buf *b;
    char status;
  } info[NUM];

  struct buf *disk_queue;
  struct spinlock vdisk_lock;
} disk;

void virtio_disk_init(void) {
  uint32 status = 0;

  initlock(&disk.vdisk_lock, "virtio_disk");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 1 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio disk");
  }

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // check FEATURES_OK ... (Legacy 模式其实可选，但留着无妨)

  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

  // 【移除】QueueReady 检查
  // if(*R(VIRTIO_MMIO_QUEUE_READY))
  //   panic("virtio disk should not be ready");

  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");

  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // 【新增2】 关键！告诉设备页大小是 4096
  // 如果不写这个，QEMU 不知道如何计算 PFN 的物理地址
  *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

  memset(disk.pages, 0, sizeof(disk.pages));
  
  // 写入物理页号
  *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.pages) >> PGSHIFT;

  disk.desc = (struct virtq_desc *) disk.pages;
  disk.avail = (struct virtq_avail *)(disk.pages + NUM*sizeof(struct virtq_desc));
  disk.used = (struct virtq_used *) (disk.pages + PGSIZE);

  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  // 【移除】QueueReady 写入
  // *R(VIRTIO_MMIO_QUEUE_READY) = 1;

  // 最后设置 DRIVER_OK
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  printf("virtio_disk_init: disk initialized\n");
}

static int alloc_desc() {
  for(int i = 0; i < NUM; i++){
    if(disk.free[i]){
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

static void free_desc(int i) {
  if(i >= NUM) panic("free_desc 1");
  if(disk.free[i]) panic("free_desc 2");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
  wakeup(&disk.free[0]);
}

static void free_chain(int i) {
  while(1){
    int flag = disk.desc[i].flags;
    int nxt = disk.desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

static int alloc3_desc(int *idx) {
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

void virtio_disk_rw(struct buf *b, int write) {
  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&disk.vdisk_lock);

  int idx[3];
  while(1){
    if(alloc3_desc(idx) == 0) {
      break;
    }
    sleep(&disk.free[0], &disk.vdisk_lock);
  }

  struct virtio_blk_req {
    uint32 type;
    uint32 reserved;
    uint64 sector;
  } buf0;

  if(write) buf0.type = VIRTIO_BLK_T_OUT;
  else buf0.type = VIRTIO_BLK_T_IN;
  buf0.reserved = 0;
  buf0.sector = sector;

  disk.desc[idx[0]].addr = (uint64) &buf0;
  disk.desc[idx[0]].len = sizeof(buf0);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = (uint64) b->data;
  disk.desc[idx[1]].len = BSIZE;
  if(write) disk.desc[idx[1]].flags = 0;
  else disk.desc[idx[1]].flags = VRING_DESC_F_WRITE;
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0;
  disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
  disk.desc[idx[2]].next = 0;

  b->disk = 1; 
  disk.info[idx[0]].b = b;

  disk.avail->ring[disk.avail->idx % NUM] = idx[0];
  __sync_synchronize();
  disk.avail->idx += 1;
  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; 
  uint64_t cycle_count = 0;
  // 【关键修复】轮询/休眠混合模式
  while(b->disk == 1) {
    // 【修改点】暂时注释掉 sleep 逻辑，强制使用轮询
    // if(current_proc) {
    //    sleep(b, &disk.vdisk_lock);
    // } else {
       
        // 即使有进程，也必须手动去查磁盘好了没，因为现在没有中断来通知我们
        release(&disk.vdisk_lock);
        virtio_disk_intr(); // 手动检查中断状态
        acquire(&disk.vdisk_lock);
    // }
        // --- 调试代码开始 ---
        cycle_count++;
        if (cycle_count % 100000 == 0) {
            printf("virtio_disk_rw: waiting... used_idx=%d, disk.used->idx=%d\n", 
                   disk.used_idx, disk.used->idx);
        }
  }

  disk.info[idx[0]].b = 0;
  free_chain(idx[0]);

  release(&disk.vdisk_lock);
}

void virtio_disk_intr() {
  acquire(&disk.vdisk_lock);
  while(disk.used_idx != disk.used->idx){
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;
    disk.used_idx += 1;

    struct buf *b = disk.info[id].b;
    b->disk = 0; 
    wakeup(b);
  }
  release(&disk.vdisk_lock);
}