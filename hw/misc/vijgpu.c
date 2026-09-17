/*
 * vijGPU - Minimal virtual PCI GPU device for QEMU 11.0.0
 *
 * First milestone: expose a PCI display controller to the guest so that
 * the guest OS can enumerate it.  No rendering, no VRAM, no DMA, no IRQ,
 * no MSI, no threads, no physical GPU access.
 *
 * PCI identity:
 *   Vendor  : 0x1234
 *   Device  : 0x1200
 *   Class   : PCI_CLASS_DISPLAY_OTHER (0x0380)
 *   Revision: 0x01
 *   BAR0    : 4 KiB MMIO (status / control registers)
 *
 * Usage:
 *   -device vijgpu
 *
 * Copyright (c) 2024 vijGPU project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pci_ids.h"
#include "qom/object.h"
#include "qapi/error.h"

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define TYPE_VIJGPU_DEVICE  "vijgpu"

#define VIJGPU_VENDOR_ID    0x1234
#define VIJGPU_DEVICE_ID    0x1200
#define VIJGPU_REVISION     0x01

/* BAR0 size: 4 KiB */
#define VIJGPU_MMIO_SIZE    0x1000

/*
 * Register map (BAR0-relative offsets)
 *
 * 0x00  VIJGPU_REG_ID       RO  Device identification magic (0x56494A47 = "VIJG")
 * 0x04  VIJGPU_REG_STATUS   RO  Device status (always 0x01 = ready)
 * 0x08  VIJGPU_REG_CONTROL  RW  Control register (guest-writable, no effect yet)
 */
#define VIJGPU_REG_ID       0x00
#define VIJGPU_REG_STATUS   0x04
#define VIJGPU_REG_CONTROL  0x08

#define VIJGPU_ID_MAGIC     0x56494a47u  /* "VIJG" */
#define VIJGPU_STATUS_READY 0x00000001u

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

OBJECT_DECLARE_SIMPLE_TYPE(VijGPUState, VIJGPU_DEVICE)

struct VijGPUState {
    PCIDevice parent_obj;

    MemoryRegion bar0;      /* 4 KiB MMIO */
    uint32_t     control;   /* VIJGPU_REG_CONTROL shadow */
};

/* ------------------------------------------------------------------ */
/* MMIO callbacks                                                      */
/* ------------------------------------------------------------------ */

static uint64_t vijgpu_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    VijGPUState *s = opaque;

    switch (addr) {
    case VIJGPU_REG_ID:
        return VIJGPU_ID_MAGIC;
    case VIJGPU_REG_STATUS:
        return VIJGPU_STATUS_READY;
    case VIJGPU_REG_CONTROL:
        return s->control;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "vijgpu: read from unknown register 0x%"HWADDR_PRIx"\n",
                      addr);
        return 0;
    }
}

static void vijgpu_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                              unsigned size)
{
    VijGPUState *s = opaque;

    switch (addr) {
    case VIJGPU_REG_CONTROL:
        s->control = (uint32_t)val;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "vijgpu: write to unknown register 0x%"HWADDR_PRIx
                      " value 0x%"PRIx64"\n",
                      addr, val);
        break;
    }
}

static const MemoryRegionOps vijgpu_mmio_ops = {
    .read       = vijgpu_mmio_read,
    .write      = vijgpu_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* ------------------------------------------------------------------ */
/* PCI realize / unrealize                                             */
/* ------------------------------------------------------------------ */

static void vijgpu_realize(PCIDevice *pdev, Error **errp)
{
    VijGPUState *s = VIJGPU_DEVICE(pdev);

    memory_region_init_io(&s->bar0, OBJECT(s), &vijgpu_mmio_ops, s,
                          "vijgpu-bar0", VIJGPU_MMIO_SIZE);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->bar0);
}

/* ------------------------------------------------------------------ */
/* QOM / class init                                                    */
/* ------------------------------------------------------------------ */

static void vijgpu_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass    *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->realize    = vijgpu_realize;
    pc->vendor_id  = VIJGPU_VENDOR_ID;
    pc->device_id  = VIJGPU_DEVICE_ID;
    pc->revision   = VIJGPU_REVISION;
    pc->class_id   = PCI_CLASS_DISPLAY_OTHER;

    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
    dc->desc = "vijGPU virtual PCI display controller";
}

static const TypeInfo vijgpu_type_info = {
    .name          = TYPE_VIJGPU_DEVICE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(VijGPUState),
    .class_init    = vijgpu_class_init,
    .interfaces    = (const InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void vijgpu_register_types(void)
{
    type_register_static(&vijgpu_type_info);
}

type_init(vijgpu_register_types)
