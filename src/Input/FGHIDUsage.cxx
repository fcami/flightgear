// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 James Turner <james@flightgear.org>

#include "config.h"

#include <simgear/debug/debug_types.h>
#include <simgear/debug/logstream.hxx>

#include "FGHIDUsage.hxx"

#include <sstream>

namespace HID {

std::pair<UsagePage, uint32_t> usage(uint32_t u)
{
    return std::make_pair(static_cast<UsagePage>((u & 0xFFFF0000) >> 16), u & 0x0000FFFF);
}

std::string nameForUsage(uint32_t usagePage, uint32_t usage)
{
    const auto enumUsage = static_cast<UsagePage>(usagePage);
    if (enumUsage == UsagePage::Undefined) {
        std::stringstream os;
        os << "undefined-" << usage;
        return os.str();
    }

    if (enumUsage == UsagePage::GenericDesktop) {
        switch (usage) {
        case GD_Undefined: return "undefined";
        case GD_Pointer: return "pointer";
        case GD_Mouse: return "mouse";
        case GD_Reserved03: return "reserved03";
        case GD_GamePad: return "gamepad";
        case GD_Keyboard: return "keyboard";
        case GD_Keypad: return "keypad";
        case GD_Joystick: return "joystick";
        case GD_Wheel: return "wheel";
        case GD_Dial: return "dial";
        case GD_Hatswitch: return "hat";
        case GD_Slider: return "slider";
        case GD_Rx: return "x-rotate";
        case GD_Ry: return "y-rotate";
        case GD_Rz: return "z-rotate";
        case GD_X: return "x-translate";
        case GD_Y: return "y-translate";
        case GD_Z: return "z-translate";
        case GD_WaterCoolingDevice: return "watercoolingdevice";
        case GD_MultiAxisController: return "multiaxiscontroller";
        case GD_TabletPCSysCtrls: return "tabletpcsysctrls";
        case GD_CountedBuffer: return "countedbuffer";
        case GD_ByteCount: return "bytecount";
        case GD_MotionWakeUp: return "motionwakeup";
        case GD_Start: return "start";
        case GD_Select: return "select";
        case GD_Vx: return "x-vector";
        case GD_Vy: return "y-vector";
        case GD_Vz: return "z-vector";
        case GD_Vbrx: return "relative-x-vector";
        case GD_Vbry: return "relative-y-vector";
        case GD_Vbrz: return "relative-z-vector";
        case GD_Vno: return "non-oriented-vector";
        case GD_DpadUp: return "direction-pad-up";
        case GD_DpadDown: return "direction-pad-down";
        case GD_DpadRight: return "direction-pad-right";
        case GD_DpadLeft: return "direction-pad-left";
        case GD_ComputerChassisDevice: return "computerchassisdevice";
        case GD_WirelessRadioControls: return "wirelessradiocontrols";
        case GD_PortableDeviceControl: return "portabledevicecontrol";
        case GD_SystemMultiAxisController: return "systemmultiaxiscontroller";
        case GD_SpatialController: return "spatialcontroller";
        case GD_AssistiveControl: return "assistivecontrol";
        case GD_DeviceDock: return "devicedock";
        case GD_DockableDevice: return "dockabledevice";
        case GD_CallStateManagementControl: return "callstatemanagementcontrol";
        case GD_FeatureNotification: return "featurenotification";
        case GD_ResolutionMultiplier: return "resolutionmultiplier";
        case GD_Qx: return "qx";
        case GD_Qy: return "qy";
        case GD_Qz: return "qz";
        case GD_Qw: return "qw";
        case GD_SystemControl: return "systemcontrol";
        case GD_SystemPowerDown: return "systempowerdown";
        case GD_SystemSleep: return "systemsleep";
        case GD_SystemWakeUp: return "systemwakeup";
        case GD_SystemContextMenu: return "systemcontextmenu";
        case GD_SystemMainMenu: return "systemmainmenu";
        case GD_SystemAppMenu: return "systemappmenu";
        case GD_SystemMenuHelp: return "systemmenuhelp";
        case GD_SystemMenuExit: return "systemmenuexit";
        case GD_SystemMenuSelect: return "systemmenuselect";
        case GD_SystemMenuRight: return "systemmenuright";
        case GD_SystemMenuLeft: return "systemmenuleft";
        case GD_SystemMenuUp: return "systemmenuup";
        case GD_SystemMenuDown: return "systemmenudown";
        case GD_SystemColdRestart: return "systemcoldrestart";
        case GD_SystemWarmRestart: return "systemwarmrestart";
        case GD_IndexTrigger: return "indextrigger";
        case GD_PalmTrigger: return "palmtrigger";
        case GD_Thumbstick: return "thumbstick";
        case GD_SystemFunctionShift: return "systemfunctionshift";
        case GD_SystemFunctionShiftLock: return "systemfunctinshiftlock";
        case GD_SystemFunctionShiftLockIndicator: return "systemfunctionshiftlockindicator";
        case GD_SystemDismissNotification: return "systemdismissnotification";
        case GD_SystemDoNotDisturb: return "systemdonotdisturb";
        case GD_SystemDock: return "systemdock";
        case GD_SystemUndock: return "systemundock";
        case GD_SystemSetup: return "systemsetup";
        case GD_SystemBreak: return "systembreak";
        case GD_SystemDebuggerBreak: return "systemdebuggerbreak";
        case GD_ApplicationBreak: return "applicationbreak";
        case GD_ApplicationDebuggerBreak: return "applicationdebuggerbreak";
        case GD_SystemSpeakerMute: return "systemspeakermute";
        case GD_SystemHibernate: return "systemhibernate";
        case GD_SystemMicrophoneMute: return "systemmicrophonemute";
        case GD_SystemDisplayInvert: return "systemdisplayinvert";
        case GD_SystemDisplayInternal: return "systemdisplayinternal";
        case GD_SystemDisplayExternal: return "systemdisplayexternal";
        case GD_SystemDisplayBoth: return "systemdisplayboth";
        case GD_SystemDisplayDual: return "systemdisplaydual";
        case GD_SystemDisplayToggleIntExtMode: return "systemdisplaytoggleintextmode";
        case GD_SystemDisplaySwapPrimarySecondary: return "systemdisplayswapprimarysecondary";
        case GD_SystemDisplayToggleLCDAutoscale: return "systemdisplaytogglelcdautoscale";
        case GD_SensorZone: return "SENSORZONE";
        case GD_RPM: return "rpm";
        case GD_CoolantLevel: return "coolantlevel";
        case GD_CoolantCriticalLevel: return "coolantcriticallevel";
        case GD_CoolantPump: return "coolant";
        case GD_ChassisEnclosure: return "chassisenclosure";
        case GD_WirelessRadioButton: return "wirelessradiobutton";
        case GD_WirelessRadioLED: return "wirelessradioled";
        case GD_WirelessRadioSliderSwitch: return "wirelessradiosliderswitch";
        case GD_SystemDisplayRotationLockButton: return "systemdisplayrotationlockbutton";
        case GD_SystemDisplayRotationLockSliderSwitch: return "systemdisplayrotationlocksliderswitch";
        case GD_ControlEnable: return "controlenable";
        case GD_DockableDeviceUniqueID: return "dockabledeviceuniqueid";
        case GD_DockableDeviceVendorID: return "dockabledevicevendorid";
        case GD_DockableDevicePrimaryUsagePage: return "dockabledeviceprimaryusagepage";
        case GD_DockableDevicePrimaryUsageID: return "dockabledeviceprimaryusageid";
        case GD_DockableDeviceDockingState: return "dockabledevicedockingstate";
        case GD_DockableDeviceDisplayOcclusion: return "dockabledevicedisplayocclusion";
        case GD_DockableDeviceObjectType: return "dockabledeviceobjecttype";
        case GD_CallActiveLED: return "callactiveled";
        case GD_CallMuteToggle: return "callmutetoggle";
        case GD_CallMuteLED: return "callmuteled";
        case GD_Reserved14: return "reserved14";
        case GD_Reserved15: return "reserved15";
        case GD_Reserved16: return "reserved16";
        case GD_Reserved17: return "reserved17";
        case GD_Reserved18: return "reserved18";
        case GD_Reserved19: return "reserved19";
        case GD_Reserved1A: return "reserved1a";
        case GD_Reserved1B: return "reserved1b";
        case GD_Reserved1C: return "reserved1c";
        case GD_Reserved1D: return "reserved1d";
        case GD_Reserved1E: return "reserved1e";
        case GD_Reserved1F: return "reserved1f";
        case GD_Reserved20: return "reserved20";
        case GD_Reserved21: return "reserved21";
        case GD_Reserved22: return "reserved22";
        case GD_Reserved23: return "reserved23";
        case GD_Reserved24: return "reserved24";
        case GD_Reserved25: return "reserved25";
        case GD_Reserved26: return "reserved26";
        case GD_Reserved27: return "reserved27";
        case GD_Reserved28: return "reserved28";
        case GD_Reserved29: return "reserved29";
        case GD_Reserved2A: return "reserved2a";
        case GD_Reserved2B: return "reserved2b";
        case GD_Reserved2C: return "reserved2c";
        case GD_Reserved2D: return "reserved2d";
        case GD_Reserved2E: return "reserved2e";
        case GD_Reserved2F: return "reserved2f";
        case GD_Reserved3F: return "reserved3f";

        default:
            SG_LOG(SG_INPUT, SG_WARN, "Unhandled HID generic desktop usage:" << usage);
        }
    } else if (enumUsage == UsagePage::Simulation) {
        switch (usage) {
        case SC_FlightSimulationDevice: return "flightsimulationdevice";
        case SC_AutomobileSimulationDevice: return "AutomobileSimulationDevice";
        case SC_TankSimulationDevice: return "tanksimulationdevice";
        case SC_SpaceShipSimulationDevice: return "spaceshipsimulationdevice";
        case SC_SubmarineSimulationDevice: return "submarinesimulationdevice";
        case SC_SailingSimulationDevice: return "sailingsimulationdevice";
        case SC_MotorcycleSimulationDevice: return "motorcyclesimulationdevice";
        case SC_SportsSimulationDevice: return "sportssimulationdevice";
        case SC_AirplaneSimulationDevice: return "airplanesimulationdevice";
        case SC_HelicopterSimulationDevice: return "helicoptersimulationdevice";
        case SC_MagicCarpetSimulationDevice: return "magiccarpetsimulationdevice";
        case SC_BycicleSimulationDevice: return "byciclesimulationdevice";
        case SC_FlightControlStick: return "flightcontrolstick";
        case SC_FlightStick: return "flightstick";
        case SC_CyclicControl: return "cycliccontrol";
        case SC_CyclicTrim: return "cyclictrim";
        case SC_FlightYoke: return "flightyoke";
        case SC_TrackControl: return "trackcontrol";
        case SC_Aileron: return "aileron";
        case SC_AileronTrim: return "ailerontrim";
        case SC_AntiTorqueControl: return "antitorquecontrol";
        case SC_AutopilotEnable: return "autopilotenable";
        case SC_ChaffRelease: return "chaffrelease";
        case SC_CollectiveControl: return "collectivecontrol";
        case SC_DiveBrake: return "divebrake";
        case SC_ElectronicCountermeasures: return "electroniccountermeasures";
        case SC_Elevator: return "elevator";
        case SC_ElevatorTrim: return "elevatortrim";
        case SC_Rudder: return "rudder";
        case SC_Throttle: return "throttle";
        case SC_FlightCommunications: return "flightcommunications";
        case SC_FlareRelease: return "flarerelease";
        case SC_LandingGear: return "landinggear";
        case SC_ToeBrake: return "toebrake";
        case SC_Trigger: return "trigger";
        case SC_WeaponsArm: return "weaponsarm";
        case SC_WeaponsSelect: return "weaponsselect";
        case SC_WingFlaps: return "wingsflap";
        case SC_Accelerator: return "accelerator";
        case SC_Brake: return "brake";
        case SC_Clutch: return "clutch";
        case SC_Shifter: return "shifter";
        case SC_Steering: return "steering";
        case SC_TurretDirection: return "turretdirection";
        case SC_BarrelElevation: return "barrelelevation";
        case SC_DivePlane: return "diveplane";
        case SC_Ballast: return "balast";
        case SC_BicycleCrank: return "bicyclehandle";
        case SC_HandleBars: return "handlebars";
        case SC_FrontBrake: return "frontbrake";
        case SC_RearBrake: return "rearbrake";
        default:
            SG_LOG(SG_INPUT, SG_WARN, "Unhandled HID simulation usage:" << usage);
        }
    } else if (enumUsage == UsagePage::Consumer) {
        switch (usage) {
        default:
            SG_LOG(SG_INPUT, SG_WARN, "Unhandled HID consumer usage:" << usage);
        }
    } else if (enumUsage == UsagePage::AlphanumericDisplay) {
        switch (usage) {
        case AD_AlphanumericDisplay: return "alphanumeric";
        case AD_CharacterReport: return "character-report";
        case AD_DisplayData: return "display-data";
        case AD_DisplayBrightness: return "display-brightness";
        case AD_7SegmentDirectMap: return "seven-segment-direct";
        case AD_14SegmentDirectMap: return "fourteen-segment-direct";

        default:
            SG_LOG(SG_INPUT, SG_WARN, "Unhandled HID alphanumeric usage:" << usage);
        }
    } else if (enumUsage == UsagePage::VR) {
        switch (usage) {
        case VR_Undefined: return "undefined-vr";
        case VR_Belt: return "belt-vr";
        case VR_BodySuit: return "bodysuit-vr";
        case VR_Flexor: return "flexor-vr";
        case VR_Glove: return "glove-vr";
        case VR_HeadTracker: return "headtracker-vr";
        case VR_HeadMountedDisplay: return "headmounteddisplay-vr";
        case VR_HandTracker: return "handtracker-vr";
        case VR_Oculometer: return "oculometer-vr";
        case VR_Vest: return "vest-vr";
        case VR_AnimatronicDevice: return "animatronicdevice-vr";
        case VR_StereoEnable: return "stereoenable-vr";
        case VR_DisplayEnable: return "displayenable-vr";
        default:
            SG_LOG(SG_INPUT, SG_WARN, "Unhandled HID VR usage:" << usage);
        }
    } else if (enumUsage == UsagePage::LEDs) {
        switch (usage) {
        case LED_Undefined: return "undefined-led";
        case LED_NumLock: return "numlock-led";
        case LED_CapsLock: return "capslock-led";
        case LED_ScrollLock: return "scrolllock-led";
        case LED_Compose: return "compose-led";
        case LED_Kana: return "kana-led";
        case LED_Power: return "power-led";
        case LED_Shift: return "shift-led";
        case LED_DoNotDisturb: return "donotdisturb-led";
        case LED_Mute: return "mute-led";
        case LED_ToneEnable: return "toneenable-led";
        case LED_HighCutFilter: return "highcutfilter-led";
        case LED_LowCutFilter: return "lowcutfilter-led";
        case LED_EqualizerEnable: return "equalizerenable-led";
        case LED_SoundFieldOn: return "soundfieldon-led";
        case LED_SurroundOn: return "surroundon-led";
        case LED_Repeat: return "repeat-led";
        case LED_Stereo: return "stereo-led";
        case LED_SampligRateDetect: return "samplingratedetect-led";
        case LED_Spinning: return "spinning-led";
        case LED_CAV: return "cav-led";
        case LED_CLV: return "clv-led";
        case LED_RecordingFormatDetect: return "recordingformatdetect-led";
        case LED_OffHook: return "offhook-led";
        case LED_Ring: return "ring-led";
        case LED_MessageWaiting: return "messagewaiting-led";
        case LED_DataMode: return "datamode-led";
        case LED_BatteryOperation: return "batteryoperation-led";
        case LED_BatteryOk: return "batteryok-led";
        case LED_BatteryLow: return "batterylow-led";
        case LED_Speaker: return "speaker-led";
        case LED_HeadSet: return "headset-led";
        case LED_Hold: return "hold-led";
        case LED_Microphone: return "microphone-led";
        case LED_Coverage: return "coverage-led";
        case LED_NightMode: return "nightmode-led";
        case LED_SendCalls: return "sendcalls-led";
        case LED_CallPickup: return "callpickup-led";
        case LED_Conference: return "conference-led";
        case LED_StandBy: return "standby-led";
        case LED_CameraOn: return "cameraon-led";
        case LED_CameraOff: return "cameraoff-led";
        case LED_OnLine: return "online-led";
        case LED_OffLine: return "offline-led";
        case LED_Busy: return "busy-led";
        case LED_Ready: return "ready-led";
        case LED_PaperOut: return "paperout-led";
        case LED_PaperJam: return "paperjam-led";
        case LED_Remote: return "remote-led";
        case LED_Forward: return "forward-led";
        case LED_Reverse: return "reverse-led";
        case LED_Stop: return "stop=led";
        case LED_Rewind: return "rewind-led";
        case LED_FastForward: return "fastforward-led";
        case LED_Play: return "play-led";
        case LED_Pause: return "pause-led";
        case LED_Record: return "record-led";
        case LED_Error: return "error-led";
        case LED_UsageSelectedIndicator: return "usageselectedindicator-led";
        case LED_UsageInUseIndicator: return "usageinuseindicator-led";
        case LED_UsageMultiModeIndicator: return "usagemultimodeindicator-led";
        case LED_IndicatorOn: return "indicatoron-led";
        case LED_IndicatorFlash: return "idicatorflash-led";
        case LED_IndicatorSlowBlink: return "indicatorslowblink-led";
        case LED_IndicatorFastBlink: return "indicatorfastblink-led";
        case LED_IndicatorOff: return "indicatoroff-led";
        case LED_FlashOnTime: return "flashontime-led";
        case LED_SlowBlinkOnTime: return "slowblinkontime-led";
        case LED_SlowBlinkOffTime: return "slowblinkofftime-led";
        case LED_FastBlinkOnTime: return "fastblinkontime-led";
        case LED_FastBlinkOfftime: return "fastblinkofftime-led";
        case LED_UsageIndicatorColor: return "usageindicatorcolor-led";
        case LED_IndicatorRed: return "usageindicatorred-led";
        case LED_IndicatorGreen: return "usageindicatorgreen-led";
        case LED_IndicatorAmber: return "usageindicatoramber-led";
        case LED_GenericIndicator: return "usagegenericindicator-led";
        case LED_SystemSuspend: return "usagesystemsuspend-led";
        case LED_ExternalPowerConnected: return "externalpowerconnected-led";
        default:
            SG_LOG(SG_INPUT, SG_WARN, "Unhandled HID LED usage:" << usage);
        }
    } else if (enumUsage == UsagePage::Button) {
        std::stringstream os;
        os << "button-" << usage;
        return os.str();
    } else if (enumUsage >= UsagePage::VendorDefinedStart) {
        return "vendor";
    } else {
        SG_LOG(SG_INPUT, SG_WARN, "Unhandled HID usage page:" << std::hex << usagePage << " with usage " << std::hex << usage);
    }

    return "unknown";
}

} // namespace HID
