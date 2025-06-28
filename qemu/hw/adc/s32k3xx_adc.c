
#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/include/hw/adc/stm32f2xx_adc.h"
#include "hw/irq.h"


#define ADC_MAX_VALUE 0xFFF  // Massimo valore dell'ADC (12-bit)

#define ADC_LOG(fmt, ...) qemu_log("%s: " fmt, __func__, ## __VA_ARGS__) //questo è ganzo, puoi linkare il log di qemu ad una call ADC_LOG- utile

static uint32_t s32k3xx_adc_generate_value(S32K3XXADCState *s) {
  //scegliere l'implementazione. Come facciamo?
    s->data = (s->data + 20) & ADC_MAX_VALUE; // Cambia il valore ad ogni lettura.
    return s->data;
}

/*
 * Legge dai registri dell'ADC.
 */
static uint64_t s32k3xx_adc_read(void *opaque, hwaddr addr, unsigned int size) {
    qemu_log("ADC READ, addr %ld\n", addr);

    S32K3XXADCState *s = opaque;
    switch (addr) {
    case S32K3XX_ADC_CTRL:
        return s->ctrl;
    case S32K3XX_ADC_CFG:
        return s->cfg;
    case S32K3XX_ADC_DATA:
        if ((s->ctrl & S32K3XX_ADC_ENABLE) && (s->ctrl & S32K3XX_ADC_START)) {
            qemu_log("ADC correctly configured for read\n");
            s->ctrl &= ~S32K3XX_ADC_START; // Reset del bit START dopo la lettura
            uint32_t result = s32k3xx_adc_generate_value(s);
            qemu_irq_pulse(s->irq); // Segnala interruzione (conversione finita)
            return result;
        } else {
            return s->data;
        }
    default:
        ADC_LOG("Accesso errato: 0x%" HWADDR_PRIx "\n", addr);
        return 0;
    }
}


static void s32k3xx_adc_write(void *opaque, hwaddr addr, uint64_t value, unsigned int size) {
    S32K3XXADCState *s = opaque;
    switch (addr) {
    case S32K3XX_ADC_CTRL:
        s->ctrl = value;
        ADC_LOG("CTRL scritto: 0x%x\n", s->ctrl);
        break;
    case S32K3XX_ADC_CFG:
        s->cfg = value;
        ADC_LOG("CFG scritto: 0x%x\n", s->cfg);
        break;
    case S32K3XX_ADC_DATA:
        ADC_LOG("Tentativo di scrittura su un registro di sola lettura\n");
        break;
    default:
        ADC_LOG("Accesso errato in scrittura: 0x%" HWADDR_PRIx "\n", addr);
        break;
    }
}

static const MemoryRegionOps s32k3xx_adc_ops = {
    .read = s32k3xx_adc_read,
    .write = s32k3xx_adc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

/*
 * Reset dell'ADC
 */
static void s32k3xx_adc_reset(DeviceState *dev) {
    S32K3XXADCState *s = S32K3XX_ADC(dev);
    s->ctrl = 0;
    s->cfg = 0;
    s->data = 0;
    qemu_log("ADC reset\n");

}

static const VMStateDescription vmstate_s32k3xx_adc = {
    .name = TYPE_S32K3XX_ADC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(ctrl, S32K3XXADCState),
        VMSTATE_UINT32(cfg, S32K3XXADCState),
        VMSTATE_UINT32(data, S32K3XXADCState),
        VMSTATE_END_OF_LIST()
    }
};

static void s32k3xx_adc_init(Object *obj) {
    S32K3XXADCState *s = S32K3XX_ADC(obj);
    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
    memory_region_init_io(&s->mmio, obj, &s32k3xx_adc_ops, s, TYPE_S32K3XX_ADC, 0x0C);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
    qemu_log("ADC Initialized\n");

}

static void s32k3xx_adc_class_init(ObjectClass *klass, void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    device_class_set_legacy_reset(dc, s32k3xx_adc_reset);
    dc->vmsd = &vmstate_s32k3xx_adc;
}

static const TypeInfo s32k3xx_adc_info = {
    .name          = TYPE_S32K3XX_ADC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(S32K3XXADCState),
    .instance_init = s32k3xx_adc_init,
    .class_init    = s32k3xx_adc_class_init,
};

static void s32k3xx_adc_register_types(void) {
    type_register_static(&s32k3xx_adc_info);
}

type_init(s32k3xx_adc_register_types)
