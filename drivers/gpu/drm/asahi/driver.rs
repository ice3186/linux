// SPDX-License-Identifier: GPL-2.0-only OR MIT

//! Top-level GPU driver implementation.

use core::ops::Deref;

use kernel::{
    c_str,
    device::{
        Core,
        DeviceContext, //
    },
    dma::{
        Device,
        DmaMask, //
    },
    drm,
    drm::ioctl,
    of,
    platform,
    prelude::*,
    sync::{
        aref::ARef,
        Arc,
        SetOnce, //
    }, //
};

use crate::{
    debug,
    file,
    gem::AsahiObject,
    gpu,
    hw,
    regs, //
};

use kernel::macros::vtable;

/// The initialized per-device data.
pub(crate) struct AsahiDataInner {
    pub(crate) gpu: Arc<dyn gpu::GpuManager>,
    pub(crate) _pdev: ARef<platform::Device>,
    pub(crate) resources: regs::Resources,
}

/// Holds the per-device data while allowing the DRM device to be created first.
pub(crate) struct AsahiData(SetOnce<AsahiDataInner>);

impl AsahiData {
    fn uninitialized() -> Self {
        Self(SetOnce::new())
    }

    /// Initialize the device data after the DRM device and GPU manager have been constructed.
    fn initialize(&self, data: AsahiDataInner) {
        assert!(
            self.0.populate(data),
            "Asahi device data initialized more than once"
        );
    }
}

impl Deref for AsahiData {
    type Target = AsahiDataInner;

    fn deref(&self) -> &Self::Target {
        self.0.as_ref().expect("Asahi device data is uninitialized")
    }
}

pub(crate) struct AsahiDriver;

/// Convenience type alias for the DRM device type for this driver.
pub(crate) type AsahiDevice<Ctx = drm::Normal> = drm::device::Device<AsahiDriver, Ctx>;
pub(crate) type AsahiDevRef = ARef<AsahiDevice>;

#[pin_data(PinnedDrop)]
pub(crate) struct AsahiDriverData<'bound> {
    _drm: AsahiDevRef,
    _registration: drm::Registration<'bound, AsahiDriver>,
}

#[pinned_drop]
impl PinnedDrop for AsahiDriverData<'_> {
    fn drop(self: Pin<&mut Self>) {}
}

/// DRM Driver metadata
const INFO: drm::driver::DriverInfo = drm::driver::DriverInfo {
    major: 0,
    minor: 0,
    patchlevel: 0,
    name: c_str!("asahi"),
    desc: c_str!("Apple AGX Graphics"),
};

/// DRM Driver implementation for `AsahiDriver`.
#[vtable]
impl drm::driver::Driver for AsahiDriver {
    /// Our `DeviceData` type, reference-counted
    type Data = AsahiData;
    type RegistrationData<'a> = ();
    /// Our `File` type.
    type File = file::File;
    /// Our `Object` type.
    type Object = drm::gem::shmem::Object<AsahiObject>;
    type ParentDevice<Ctx: DeviceContext> = platform::Device<Ctx>;

    const INFO: drm::driver::DriverInfo = INFO;
    const FEAT_RENDER: bool = true;
    const FEAT_SYNCOBJ: bool = true;
    const FEAT_SYNCOBJ_TIMELINE: bool = true;

    kernel::declare_drm_ioctls! {
        (ASAHI_GET_PARAMS,      drm_asahi_get_params,
                          ioctl::RENDER_ALLOW, crate::file::File::get_params),
        (ASAHI_GET_TIME,        drm_asahi_get_time,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::get_time),
        (ASAHI_VM_CREATE,       drm_asahi_vm_create,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::vm_create),
        (ASAHI_VM_DESTROY,      drm_asahi_vm_destroy,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::vm_destroy),
        (ASAHI_VM_BIND,         drm_asahi_vm_bind,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::vm_bind),
        (ASAHI_GEM_CREATE,      drm_asahi_gem_create,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::gem_create),
        (ASAHI_GEM_MMAP_OFFSET, drm_asahi_gem_mmap_offset,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::gem_mmap_offset),
        (ASAHI_GEM_BIND_OBJECT, drm_asahi_gem_bind_object,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::gem_bind_object),
        (ASAHI_QUEUE_CREATE,    drm_asahi_queue_create,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::queue_create),
        (ASAHI_QUEUE_DESTROY,   drm_asahi_queue_destroy,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::queue_destroy),
        (ASAHI_SUBMIT,          drm_asahi_submit,
            ioctl::AUTH | ioctl::RENDER_ALLOW, crate::file::File::submit),
    }
}

// OF Device ID table.s
kernel::of_device_table!(
    OF_TABLE,
    MODULE_OF_TABLE,
    <AsahiDriver as platform::Driver>::IdInfo,
    [
        (
            of::DeviceId::new(c_str!("apple,agx-t8103")),
            &hw::t8103::HWCONFIG
        ),
        (
            of::DeviceId::new(c_str!("apple,agx-t8112")),
            &hw::t8112::HWCONFIG
        ),
        (
            of::DeviceId::new(c_str!("apple,agx-t6000")),
            &hw::t600x::HWCONFIG_T6000
        ),
        (
            of::DeviceId::new(c_str!("apple,agx-t6001")),
            &hw::t600x::HWCONFIG_T6001
        ),
        (
            of::DeviceId::new(c_str!("apple,agx-t6002")),
            &hw::t600x::HWCONFIG_T6002
        ),
        (
            of::DeviceId::new(c_str!("apple,agx-t6020")),
            &hw::t602x::HWCONFIG_T6020
        ),
        (
            of::DeviceId::new(c_str!("apple,agx-t6021")),
            &hw::t602x::HWCONFIG_T6021
        ),
        (
            of::DeviceId::new(c_str!("apple,agx-t6022")),
            &hw::t602x::HWCONFIG_T6022
        ),
    ]
);

/// Platform Driver implementation for `AsahiDriver`.
impl platform::Driver for AsahiDriver {
    type IdInfo = &'static hw::HwConfig;
    type Data<'bound> = AsahiDriverData<'bound>;
    // The WIP driver does not yet have a sound teardown path.
    const SUPPRESS_BIND_ATTRS: bool = true;
    const OF_ID_TABLE: Option<of::IdTable<Self::IdInfo>> = Some(&OF_TABLE);

    /// Device probe function.
    fn probe<'bound>(
        pdev: &'bound platform::Device<Core<'_>>,
        info: Option<&'bound Self::IdInfo>,
    ) -> impl PinInit<Self::Data<'bound>, Error> + 'bound {
        debug::update_debug_flags();

        dev_info!(pdev.as_ref(), "Probing...\n");

        let cfg = info.ok_or(ENODEV)?;

        unsafe { pdev.dma_set_mask_and_coherent(DmaMask::try_new(cfg.uat_oas)?)? };

        let res = regs::Resources::new(pdev)?;

        // Initialize misc MMIO
        res.init_mmio()?;

        // Start the coprocessor CPU, so UAT can initialize the handoff
        regs::Resources::start_cpu(pdev)?;

        let fwnode = pdev.as_ref().fwnode().ok_or(EIO)?;
        let compat: KVec<u32> = fwnode
            .property_read_array_vec(c_str!("apple,firmware-compat"), 3)?
            .required_by(pdev.as_ref())?;

        let drm =
            drm::UnregisteredDevice::<AsahiDriver>::new(pdev, Ok(AsahiData::uninitialized()))?;

        let gpu = match (cfg.gpu_gen, cfg.gpu_variant, compat.as_slice()) {
            (hw::GpuGen::G13, _, &[12, 3, 0]) => {
                gpu::GpuManagerG13V12_3::new(&drm, &res, cfg)? as Arc<dyn gpu::GpuManager>
            }
            (hw::GpuGen::G14, hw::GpuVariant::G, &[12, 4, 0]) => {
                gpu::GpuManagerG14V12_4::new(&drm, &res, cfg)? as Arc<dyn gpu::GpuManager>
            }
            (hw::GpuGen::G13, _, &[13, 5, 0]) => {
                gpu::GpuManagerG13V13_5::new(&drm, &res, cfg)? as Arc<dyn gpu::GpuManager>
            }
            (hw::GpuGen::G14, hw::GpuVariant::G, &[13, 5, 0]) => {
                gpu::GpuManagerG14V13_5::new(&drm, &res, cfg)? as Arc<dyn gpu::GpuManager>
            }
            (hw::GpuGen::G14, _, &[13, 5, 0]) => {
                gpu::GpuManagerG14XV13_5::new(&drm, &res, cfg)? as Arc<dyn gpu::GpuManager>
            }
            _ => {
                dev_info!(
                    pdev.as_ref(),
                    "Unsupported GPU/firmware combination ({:?}, {:?}, {:?})\n",
                    cfg.gpu_gen,
                    cfg.gpu_variant,
                    compat
                );
                return Err(ENODEV);
            }
        };

        let data = AsahiDataInner {
            gpu,
            _pdev: pdev.into(),
            resources: res,
        };

        (*drm).initialize(data);

        if let Err(err) = (*drm).gpu.init() {
            dev_err!(
                pdev.as_ref(),
                "GPU initialization failed after callback cycles were established: {:?}\n",
                err
            );
            // The WIP manager/RTKit callback graph may now contain reference cycles. Returning
            // from probe would revoke platform devres while leaked callbacks and SG tables can
            // still access them. Fail closed; reboot is the only supported recovery.
            panic!("Asahi GPU initialization entered non-teardown-safe state; reboot required");
        }

        // SAFETY: The registration is stored in the platform driver's binding data and is
        // therefore dropped when the platform device is unbound; it is never forgotten.
        let registration = match unsafe { drm::Registration::new(pdev.as_ref(), drm, (), 0) } {
            Ok(registration) => registration,
            Err(err) => {
                dev_err!(
                    pdev.as_ref(),
                    "DRM registration failed after callback cycles were established: {:?}\n",
                    err
                );
                // As above, returning would revoke devres beneath the leaked callback graph.
                // Panic contains the unsafe state; reboot is the only supported recovery.
                panic!("Asahi DRM registration entered non-teardown-safe state; reboot required");
            }
        };
        let drm = registration.device().into();

        Ok(AsahiDriverData {
            _drm: drm,
            _registration: registration,
        })
    }
}
