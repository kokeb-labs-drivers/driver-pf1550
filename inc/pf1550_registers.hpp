/**
 * @file pf1550_registers.hpp
 * @brief PF1550 register map: addresses, bit fields, reset values and the access table (datasheet
 * section 12).
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Registers are 8 bits wide with 7-bit device addresses. The PMIC page is at 0x00 to 0x7F; the
 * charger page tables are headed with offset 0x80, so the absolute address is 0x80 plus the table
 * address (datasheet 12.2). Every register the data sheet defines has one row in `kRegisters`, with
 * the access class that decides what the driver may do with it: the driver writes only registers
 * whose class allows it, so a register that is not meant to be written (identity, status,
 * `KEY1`, `CHGR_KEY2`) cannot be reached by any call.
 */
#ifndef PF1550_REGISTERS_HPP
#define PF1550_REGISTERS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "pf1550_detail.hpp"

/** @brief The PF1550 register map. */
namespace pf1550::reg {

/**
 * @defgroup pf1550_registers Register map
 * @brief Addresses, fields, reset values and the access table.
 * @{
 */

/// @name PMIC page: identity and interrupts (Tables 86 to 110)
/// @{
inline constexpr uint8_t kDeviceId = 0x00;   ///< DEVICE_ID (R): 0x7C.
inline constexpr uint8_t kOtpFlavor = 0x01;  ///< OTP_FLAVOR (R): the start-up option.
inline constexpr uint8_t kSiliconRev = 0x02; ///< SILICON_REV (R).
inline constexpr uint8_t kIntCategory =
    0x06;                                    ///< INT_CATEGORY (R): which group has a pending event.
inline constexpr uint8_t kSwIntStat0 = 0x08; ///< SW_INT_STAT0: SWx low-side current limit.
inline constexpr uint8_t kSwIntMask0 = 0x09; ///< SW_INT_MASK0.
inline constexpr uint8_t kSwIntSense0 = 0x0A;    ///< SW_INT_SENSE0.
inline constexpr uint8_t kSwIntStat1 = 0x0B;     ///< SW_INT_STAT1: SWx high-side current limit.
inline constexpr uint8_t kSwIntMask1 = 0x0C;     ///< SW_INT_MASK1.
inline constexpr uint8_t kSwIntSense1 = 0x0D;    ///< SW_INT_SENSE1.
inline constexpr uint8_t kSwIntStat2 = 0x0E;     ///< SW_INT_STAT2: SW1 and SW2 DVS done.
inline constexpr uint8_t kSwIntMask2 = 0x0F;     ///< SW_INT_MASK2.
inline constexpr uint8_t kSwIntSense2 = 0x10;    ///< SW_INT_SENSE2.
inline constexpr uint8_t kLdoIntStat0 = 0x18;    ///< LDO_INT_STAT0: LDOx current-limit faults.
inline constexpr uint8_t kLdoIntMask0 = 0x19;    ///< LDO_INT_MASK0.
inline constexpr uint8_t kLdoIntSense0 = 0x1A;   ///< LDO_INT_SENSE0.
inline constexpr uint8_t kTempIntStat0 = 0x20;   ///< TEMP_INT_STAT0: die temperature 110 and 125 C.
inline constexpr uint8_t kTempIntMask0 = 0x21;   ///< TEMP_INT_MASK0.
inline constexpr uint8_t kTempIntSense0 = 0x22;  ///< TEMP_INT_SENSE0.
inline constexpr uint8_t kOnkeyIntStat0 = 0x24;  ///< ONKEY_INT_STAT0: push and 1, 2, 3, 4, 8 s.
inline constexpr uint8_t kOnkeyIntMask0 = 0x25;  ///< ONKEY_INT_MASK0.
inline constexpr uint8_t kOnkeyIntSense0 = 0x26; ///< ONKEY_INT_SENSE0.
inline constexpr uint8_t kMiscIntStat0 = 0x28;   ///< MISC_INT_STAT0: power-up/down, PWRON, VSYS.
inline constexpr uint8_t kMiscIntMask0 = 0x29;   ///< MISC_INT_MASK0.
inline constexpr uint8_t kMiscIntSense0 = 0x2A;  ///< MISC_INT_SENSE0.
/// @}

/// @name PMIC page: supplies (Tables 111 to 134)
/// @{
inline constexpr uint8_t kCoinCellControl = 0x30; ///< COINCELL_CONTROL: VCOIN[3:0], COINCHEN.
inline constexpr uint8_t kSw1Volt = 0x32;         ///< SW1_VOLT (run).
inline constexpr uint8_t kSw1StbyVolt = 0x33;     ///< SW1_STBY_VOLT.
inline constexpr uint8_t kSw1SlpVolt = 0x34;      ///< SW1_SLP_VOLT.
inline constexpr uint8_t kSw1Ctrl = 0x35;         ///< SW1_CTRL.
inline constexpr uint8_t kSw1Ctrl1 = 0x36; ///< SW1_CTRL1 (the data sheet titles it SW1_SLP_VOLT).
inline constexpr uint8_t kSw2Volt = 0x38;  ///< SW2_VOLT (run).
inline constexpr uint8_t kSw2StbyVolt = 0x39; ///< SW2_STBY_VOLT.
inline constexpr uint8_t kSw2SlpVolt = 0x3A;  ///< SW2_SLP_VOLT.
inline constexpr uint8_t kSw2Ctrl = 0x3B;     ///< SW2_CTRL.
inline constexpr uint8_t kSw2Ctrl1 = 0x3C;    ///< SW2_CTRL1.
inline constexpr uint8_t kSw3Volt = 0x3E;     ///< SW3_VOLT (run), read only.
inline constexpr uint8_t kSw3StbyVolt = 0x3F; ///< SW3_STBY_VOLT, read only.
inline constexpr uint8_t kSw3SlpVolt = 0x40;  ///< SW3_SLP_VOLT, read only.
inline constexpr uint8_t kSw3Ctrl = 0x41;     ///< SW3_CTRL.
inline constexpr uint8_t kSw3Ctrl1 = 0x42;    ///< SW3_CTRL1.
inline constexpr uint8_t kVsnvsCtrl = 0x48;   ///< VSNVS_CTRL.
inline constexpr uint8_t kVrefddrCtrl = 0x4A; ///< VREFDDR_CTRL.
inline constexpr uint8_t kLdo1Volt = 0x4C;    ///< LDO1_VOLT.
inline constexpr uint8_t kLdo1Ctrl = 0x4D;    ///< LDO1_CTRL.
inline constexpr uint8_t kLdo2Volt = 0x4F;    ///< LDO2_VOLT.
inline constexpr uint8_t kLdo2Ctrl = 0x50;    ///< LDO2_CTRL.
inline constexpr uint8_t kLdo3Volt = 0x52;    ///< LDO3_VOLT.
inline constexpr uint8_t kLdo3Ctrl = 0x53;    ///< LDO3_CTRL.
/// @}

/// @name PMIC page: power control and state (Tables 135 to 149)
/// @{
inline constexpr uint8_t kPwrCtrl0 = 0x58; ///< PWRCTRL0: STANDBYDLY, STANDBYINV, POR_DLY, TGRESET.
inline constexpr uint8_t kPwrCtrl1 = 0x59; ///< PWRCTRL1: debounce, reset paths, REGSCPEN.
inline constexpr uint8_t kPwrCtrl2 = 0x5A; ///< PWRCTRL2: UVDET, LOW_SYS_WARN.
inline constexpr uint8_t kPwrCtrl3 = 0x5B; ///< PWRCTRL3: GOTO_SHIP, GOTO_CORE_OFF.
inline constexpr uint8_t kSw1PwrdnSeq = 0x5F;     ///< SW1_PWRDN_SEQ[2:0].
inline constexpr uint8_t kSw2PwrdnSeq = 0x60;     ///< SW2_PWRDN_SEQ[2:0].
inline constexpr uint8_t kSw3PwrdnSeq = 0x61;     ///< SW3_PWRDN_SEQ[2:0] (Table 141 prints SW2).
inline constexpr uint8_t kLdo1PwrdnSeq = 0x62;    ///< LDO1_PWRDN_SEQ[2:0].
inline constexpr uint8_t kLdo2PwrdnSeq = 0x63;    ///< LDO2_PWRDN_SEQ[2:0].
inline constexpr uint8_t kLdo3PwrdnSeq = 0x64;    ///< LDO3_PWRDN_SEQ[2:0].
inline constexpr uint8_t kVrefddrPwrdnSeq = 0x65; ///< VREFDDR_PWRDN_SEQ[2:0].
inline constexpr uint8_t kStateInfo = 0x67;       ///< STATE_INFO (R).
inline constexpr uint8_t kI2cAddr = 0x68; ///< I2C_ADDR: I2C_SLAVE_ADDR[2:0] (R), USE_DEFAULT_ADDR.
inline constexpr uint8_t kIoDrv0 = 0x69;  ///< IO_DRV0 (bitmap only, not exposed).
inline constexpr uint8_t kIoDrv1 = 0x6A;  ///< IO_DRV1 (bitmap only, not exposed).
inline constexpr uint8_t kRc16Mhz = 0x6B; ///< RC_16MHZ (not exposed).
inline constexpr uint8_t kKey1 = 0x6F;    ///< KEY1 (bitmap; not exposed).
/// @}

/// @name Charger page (absolute address 0x80 + table address; Tables 150 to 173)
/// @{
inline constexpr uint8_t kChgInt = 0x80;        ///< CHG_INT: charger interrupt status.
inline constexpr uint8_t kChgIntMask = 0x82;    ///< CHG_INT_MASK (reset: all masked).
inline constexpr uint8_t kChgIntOk = 0x84;      ///< CHG_INT_OK (R): live state of the sources.
inline constexpr uint8_t kVbusSns = 0x86;       ///< VBUS_SNS (R).
inline constexpr uint8_t kChgSns = 0x87;        ///< CHG_SNS (R): state, WDT_SNS, THM_SNS, TREG_SNS.
inline constexpr uint8_t kBattSns = 0x88;       ///< BATT_SNS (R).
inline constexpr uint8_t kChgOper = 0x89;       ///< CHG_OPER: mode, WDTEN, DISBATFET.
inline constexpr uint8_t kChgTmr = 0x8A;        ///< CHG_TMR: FCHGTIME, EOCTIME, TPRECHG.
inline constexpr uint8_t kChgEocCnfg = 0x8D;    ///< CHG_EOC_CNFG: CHG_RESTART, IEOC.
inline constexpr uint8_t kChgCurrCnfg = 0x8E;   ///< CHG_CURR_CNFG: CHG_CC, PRECHGLB_THRS.
inline constexpr uint8_t kBattReg = 0x8F;       ///< BATT_REG: CHGCV, VSYSMIN.
inline constexpr uint8_t kBatfetCnfg = 0x91;    ///< BATFET_CNFG: WDTCLR, BATFET_OC, WD_TIME, ...
inline constexpr uint8_t kThmRegCnfg = 0x92;    ///< THM_REG_CNFG: THM_CNFG, REGTEMP, cold, hot.
inline constexpr uint8_t kVbusInlimCnfg = 0x94; ///< VBUS_INLIM_CNFG: VBUS_LIN_ILIM.
inline constexpr uint8_t kVbusLinDpm = 0x95;    ///< VBUS_LIN_DPM: VBUS_DPM_REG, VIN_DPM_STOP.
inline constexpr uint8_t kUsbPhyLdoCnfg = 0x96; ///< USB_PHY_LDO_CNFG.
inline constexpr uint8_t kDbncDelayTime = 0x98; ///< DBNC_DELAY_TIME.
inline constexpr uint8_t kChgIntCnfg = 0x99;    ///< CHG_INT_CNFG.
inline constexpr uint8_t kThmAdjSetting = 0x9A; ///< THM_ADJ_SETTING: warm, cool, CV_ADJ, CC_ADJ.
inline constexpr uint8_t kVbus2SysCnfg = 0x9B;  ///< VBUS2SYS_CNFG.
inline constexpr uint8_t kLedPwm = 0x9C;        ///< LED_PWM: duty, ramp, enable.
inline constexpr uint8_t kFaultBatfetCnfg = 0x9D; ///< FAULT_BATFET_CNFG: BATFET policy per fault.
inline constexpr uint8_t kLedCnfg = 0x9E;         ///< LED_CNFG.
inline constexpr uint8_t kChgrKey2 = 0x9F;        ///< CHGR_KEY2 (Reserved; not exposed).
/// @}

/// Identity values (Tables 86 to 88).
inline constexpr uint8_t kDeviceIdValue = 0x7C; ///< DEVICE_ID: FAMILY 01111, DEVICE_ID 100.
/// Highest address of the PMIC page.
inline constexpr uint8_t kLastPmicRegister = 0x6F;
/// Highest address of the charger page.
inline constexpr uint8_t kLastChargerRegister = 0x9F;

/// @name Bit fields (all shifts and widths from the register tables)
/// @{
using DeviceIdLow = detail::Field<uint8_t, 0U, 3U>;    ///< DEVICE_ID[2:0] = 100.
using DeviceIdFamily = detail::Field<uint8_t, 3U, 5U>; ///< DEVICE_ID[7:3] = 01111.
using CoinVcoin = detail::Field<uint8_t, 0U, 4U>;      ///< COINCELL_CONTROL.VCOIN.
inline constexpr uint8_t kCoinChen = 1U << 4;          ///< COINCELL_CONTROL.COINCHEN.
inline constexpr uint8_t kCtrlEn = 1U << 0;            ///< xxx_CTRL.EN (run).
inline constexpr uint8_t kCtrlStbyEn = 1U << 1;        ///< xxx_CTRL.STBY_EN.
inline constexpr uint8_t kCtrlOmode = 1U << 2;         ///< xxx_CTRL.OMODE (enabled in SLEEP).
inline constexpr uint8_t kCtrlLpwr = 1U << 3;          ///< xxx_CTRL.LPWR.
inline constexpr uint8_t kBuckDvsSpeed = 1U << 4;      ///< SWx_CTRL.DVSSPEED (0: 2 us, 1: 4 us).
inline constexpr uint8_t kBuckFpwmInDvs = 1U << 5;     ///< SWx_CTRL.FPWM_IN_DVS.
inline constexpr uint8_t kBuckFpwm = 1U << 6;          ///< SWx_CTRL.FPWM.
inline constexpr uint8_t kBuckRdisEnb = 1U << 7;  ///< SWx_CTRL.RDIS_ENB (1: no discharge resistor).
inline constexpr uint8_t kLdoLsEn = 1U << 4;      ///< LDOx_CTRL.LS_EN (load switch, one way).
using BuckIlim = detail::Field<uint8_t, 0U, 2U>;  ///< SWx_CTRL1.ILIM.
using BuckVolt6 = detail::Field<uint8_t, 0U, 6U>; ///< SW1/SW2 voltage code.
using BuckVolt4 = detail::Field<uint8_t, 0U, 4U>; ///< SW3 voltage code.
using Ldo5Volt = detail::Field<uint8_t, 0U, 5U>;  ///< LDO1/LDO3 voltage code.
using Ldo4Volt = detail::Field<uint8_t, 0U, 4U>;  ///< LDO2 voltage code.
inline constexpr uint8_t kVsnvsLibgdis = 1U << 5; ///< VSNVS_CTRL.LIBGDIS.
using StandbyDly = detail::Field<uint8_t, 0U, 2U>;    ///< PWRCTRL0.STANDBYDLY.
inline constexpr uint8_t kStandbyInv = 1U << 2;       ///< PWRCTRL0.STANDBYINV.
using PorDly = detail::Field<uint8_t, 3U, 3U>;        ///< PWRCTRL0.POR_DLY (OTP loaded).
using TgReset = detail::Field<uint8_t, 6U, 2U>;       ///< PWRCTRL0.TGRESET.
using PwronDbnc = detail::Field<uint8_t, 0U, 2U>;     ///< PWRCTRL1.PWRONDBNC.
using OnkeyDbnc = detail::Field<uint8_t, 2U, 2U>;     ///< PWRCTRL1.ONKEYDBNC.
inline constexpr uint8_t kPwronRstEn = 1U << 4;       ///< PWRCTRL1.PWRONRSTEN.
inline constexpr uint8_t kRestartEn = 1U << 5;        ///< PWRCTRL1.RESTARTEN.
inline constexpr uint8_t kRegScpEn = 1U << 6;         ///< PWRCTRL1.REGSCPEN.
inline constexpr uint8_t kOnkeyRstEn = 1U << 7;       ///< PWRCTRL1.ONKEY_RST_EN.
using Uvdet = detail::Field<uint8_t, 0U, 2U>;         ///< PWRCTRL2.UVDET.
using LowSysWarn = detail::Field<uint8_t, 2U, 2U>;    ///< PWRCTRL2.LOW_SYS_WARN.
inline constexpr uint8_t kGotoShip = 1U << 0;         ///< PWRCTRL3.GOTO_SHIP.
inline constexpr uint8_t kGotoCoreOff = 1U << 1;      ///< PWRCTRL3.GOTO_CORE_OFF.
using PwrdnSeq = detail::Field<uint8_t, 0U, 3U>;      ///< xxx_PWRDN_SEQ[2:0].
using StateInfo = detail::Field<uint8_t, 0U, 6U>;     ///< STATE_INFO.STATE.
using I2cSlaveAddr = detail::Field<uint8_t, 0U, 3U>;  ///< I2C_ADDR.I2C_SLAVE_ADDR[2:0].
using ChgOperMode = detail::Field<uint8_t, 0U, 2U>;   ///< CHG_OPER.CHG_OPER.
inline constexpr uint8_t kChgOperWdten = 1U << 3;     ///< CHG_OPER.WDTEN.
inline constexpr uint8_t kChgOperDisbatfet = 1U << 4; ///< CHG_OPER.DISBATFET.
using Fchgtime = detail::Field<uint8_t, 0U, 3U>;      ///< CHG_TMR.FCHGTIME.
using Eoctime = detail::Field<uint8_t, 3U, 3U>;       ///< CHG_TMR.EOCTIME.
inline constexpr uint8_t kTprechg = 1U << 7;          ///< CHG_TMR.TPRECHG (0: 30 min, 1: 45 min).
using ChgRestart = detail::Field<uint8_t, 0U, 2U>;    ///< CHG_EOC_CNFG.CHG_RESTART.
using Ieoc = detail::Field<uint8_t, 4U, 3U>;          ///< CHG_EOC_CNFG.IEOC.
using ChgCc = detail::Field<uint8_t, 0U, 5U>;         ///< CHG_CURR_CNFG.CHG_CC.
using PrechgLbThrs = detail::Field<uint8_t, 5U, 2U>;  ///< CHG_CURR_CNFG.PRECHGLB_THRS.
using Chgcv = detail::Field<uint8_t, 0U, 6U>;         ///< BATT_REG.CHGCV.
using VsysMin = detail::Field<uint8_t, 6U, 2U>;       ///< BATT_REG.VSYSMIN.
using Wdtclr = detail::Field<uint8_t, 0U, 2U>;        ///< BATFET_CNFG.WDTCLR.
inline constexpr uint8_t kBovrcDisbatfet = 1U << 3;   ///< BATFET_CNFG.BOVRC_DISBATFET.
using BatfetOc = detail::Field<uint8_t, 4U, 2U>;      ///< BATFET_CNFG.BATFET_OC.
inline constexpr uint8_t kWdTime = 1U << 6;           ///< BATFET_CNFG.WD_TIME.
inline constexpr uint8_t kBovrcNovbus = 1U << 7;      ///< BATFET_CNFG.BOVRC_NOVBUS.
using ThmCnfg = detail::Field<uint8_t, 0U, 2U>;       ///< THM_REG_CNFG.THM_CNFG.
using Regtemp = detail::Field<uint8_t, 2U, 2U>;       ///< THM_REG_CNFG.REGTEMP.
inline constexpr uint8_t kThmCold = 1U << 4;          ///< THM_REG_CNFG.THM_COLD (1: -10 C).
inline constexpr uint8_t kThmHot = 1U << 5;           ///< THM_REG_CNFG.THM_HOT (1: 55 C).
inline constexpr uint8_t kTempFbEn = 1U << 7;         ///< THM_REG_CNFG.TEMP_FB_EN.
using VbusLinIlim = detail::Field<uint8_t, 3U, 5U>;   ///< VBUS_INLIM_CNFG.VBUS_LIN_ILIM.
using VbusDpmReg = detail::Field<uint8_t, 0U, 3U>;    ///< VBUS_LIN_DPM.VBUS_DPM_REG.
inline constexpr uint8_t kVinDpmStop = 1U << 5;       ///< VBUS_LIN_DPM.VIN_DPM_STOP (1: 250 mV).
inline constexpr uint8_t kActDisPhy = 1U << 0;        ///< USB_PHY_LDO_CNFG.ACTDISPHY.
inline constexpr uint8_t kUsbPhy49 = 1U << 1;         ///< USB_PHY_LDO_CNFG.USBPHY (1: 4.9 V).
inline constexpr uint8_t kUsbPhyLdo = 1U << 2;        ///< USB_PHY_LDO_CNFG.USBPHYLDO.
using VbusOvTdb = detail::Field<uint8_t, 0U, 2U>;     ///< DBNC_DELAY_TIME.VBUS_OV_TDB.
using SysWkupDly = detail::Field<uint8_t, 4U, 2U>;    ///< DBNC_DELAY_TIME.SYS_WKUP_DLY.
inline constexpr uint8_t kChgIntGen = 1U << 0;        ///< CHG_INT_CNFG.CHG_INT_GEN.
inline constexpr uint8_t kEocInt = 1U << 1;           ///< CHG_INT_CNFG.EOC_INT.
inline constexpr uint8_t kThmWarm = 1U << 0;          ///< THM_ADJ_SETTING.THM_WARM (1: 50 C).
inline constexpr uint8_t kThmCool = 1U << 1;          ///< THM_ADJ_SETTING.THM_COOL (1: 10 C).
using CvAdj = detail::Field<uint8_t, 2U, 2U>;         ///< THM_ADJ_SETTING.CV_ADJ.
using CcAdj = detail::Field<uint8_t, 4U, 2U>;         ///< THM_ADJ_SETTING.CC_ADJ.
using Vbus2SysTdb = detail::Field<uint8_t, 0U, 2U>;   ///< VBUS2SYS_CNFG.VBUS2SYS_TDB.
inline constexpr uint8_t kVbus2SysThrsh = 1U << 2;    ///< VBUS2SYS_CNFG.VBUS2SYS_THRSH (1: 175 mV).
using LedPwmDuty = detail::Field<uint8_t, 0U, 6U>;    ///< LED_PWM.LED_PWM.
inline constexpr uint8_t kLedRamp = 1U << 6;          ///< LED_PWM.LED_RAMP.
inline constexpr uint8_t kLedEn = 1U << 7;            ///< LED_PWM.LED_EN.
inline constexpr uint8_t kOvfltBfetEn = 1U << 0;  ///< FAULT_BATFET_CNFG: BATFET stays closed on OV.
inline constexpr uint8_t kWdfltBfetEn = 1U << 1;  ///< ... on watchdog fault.
inline constexpr uint8_t kThmsusBfetEn = 1U << 2; ///< ... on thermistor fault.
inline constexpr uint8_t kTshdnBfetEn = 1U << 3;  ///< ... on thermal shutdown.
inline constexpr uint8_t kTmrfltBfetEn = 1U << 4; ///< ... on timer fault.
using LedFreq = detail::Field<uint8_t, 0U, 2U>;   ///< LED_CNFG.LED_FREQ.
using LedCurrent = detail::Field<uint8_t, 2U, 2U>; ///< LED_CNFG.LED_CURRENT (01 = 6 mA).
/// LED_CNFG.LED_CURRENT = 01 (6 mA), the only non-reserved code (Table 172), in place.
inline constexpr uint8_t kLedCurrent6mA = 1U << 2;
inline constexpr uint8_t kLedCfg = 1U << 4;          ///< LED_CNFG.LED_CFG.
inline constexpr uint8_t kLedOvrd = 1U << 5;         ///< LED_CNFG.LEDOVRD.
using ChgSnsState = detail::Field<uint8_t, 0U, 4U>;  ///< CHG_SNS.CHG_SNS.
inline constexpr uint8_t kChgSnsWdt = 1U << 5;       ///< CHG_SNS.WDT_SNS.
inline constexpr uint8_t kChgSnsThm = 1U << 6;       ///< CHG_SNS.THM_SNS.
inline constexpr uint8_t kChgSnsTreg = 1U << 7;      ///< CHG_SNS.TREG_SNS.
using BattSnsState = detail::Field<uint8_t, 0U, 3U>; ///< BATT_SNS.BATT_SNS.
inline constexpr uint8_t kBattSnsOc = 1U << 5;       ///< BATT_SNS.BATTOC_SNS.
inline constexpr uint8_t kVbusUvlo = 1U << 2;        ///< VBUS_SNS.VBUS_UVLO_SNS.
inline constexpr uint8_t kVbusIn2Sys = 1U << 3;      ///< VBUS_SNS.VBUS_IN2SYS_SNS.
inline constexpr uint8_t kVbusOvlo = 1U << 4;        ///< VBUS_SNS.VBUS_OVLO_SNS.
inline constexpr uint8_t kVbusValid = 1U << 5;       ///< VBUS_SNS.VBUS_VALID.
inline constexpr uint8_t kVbusDpm = 1U << 7;         ///< VBUS_SNS.VBUS_DPM_SNS.
/// @}

/// Access class of a register: what the driver is allowed to do with it.
enum class Access : uint8_t {
  ReadOnly, ///< Identity, state, SENSE, OK and SNS registers: never written.
  Status,   ///< Interrupt status (write 1 clears reported bits only): written by the service only.
  Mask,     ///< Interrupt mask: read-modify-write of defined bits.
  OtpCfg,   ///< Loaded from OTP at start-up, then read/write: typed setter with read-back.
  Cfg,      ///< Ordinary configuration: typed setter with read-back.
  Command,  ///< PWRCTRL3 (GOTO_SHIP, GOTO_CORE_OFF): written only by the shutdown calls; the typed
            ///< setter refuses it.
  Never,    ///< Documented, but no call writes it (reserved, keys, SW3 voltages, evaluation bits).
};

/// One row of the access table.
struct RegisterInfo {
  uint8_t address; ///< Absolute register address.
  Access access;   ///< What the driver may do.
};

/// Every documented register with its access class (matrix rows REG-xx).
inline constexpr std::array<RegisterInfo, 90> kRegisters = {{
    {.address = kDeviceId, .access = Access::ReadOnly},
    {.address = kOtpFlavor, .access = Access::ReadOnly},
    {.address = kSiliconRev, .access = Access::ReadOnly},
    {.address = kIntCategory, .access = Access::ReadOnly},
    {.address = kSwIntStat0, .access = Access::Status},
    {.address = kSwIntMask0, .access = Access::Mask},
    {.address = kSwIntSense0, .access = Access::ReadOnly},
    {.address = kSwIntStat1, .access = Access::Status},
    {.address = kSwIntMask1, .access = Access::Mask},
    {.address = kSwIntSense1, .access = Access::ReadOnly},
    {.address = kSwIntStat2, .access = Access::Status},
    {.address = kSwIntMask2, .access = Access::Mask},
    {.address = kSwIntSense2, .access = Access::ReadOnly},
    {.address = kLdoIntStat0, .access = Access::Status},
    {.address = kLdoIntMask0, .access = Access::Mask},
    {.address = kLdoIntSense0, .access = Access::ReadOnly},
    {.address = kTempIntStat0, .access = Access::Status},
    {.address = kTempIntMask0, .access = Access::Mask},
    {.address = kTempIntSense0, .access = Access::ReadOnly},
    {.address = kOnkeyIntStat0, .access = Access::Status},
    {.address = kOnkeyIntMask0, .access = Access::Mask},
    {.address = kOnkeyIntSense0, .access = Access::ReadOnly},
    {.address = kMiscIntStat0, .access = Access::Status},
    {.address = kMiscIntMask0, .access = Access::Mask},
    {.address = kMiscIntSense0, .access = Access::ReadOnly},
    {.address = kCoinCellControl, .access = Access::Cfg},
    {.address = kSw1Volt, .access = Access::OtpCfg},
    {.address = kSw1StbyVolt, .access = Access::OtpCfg},
    {.address = kSw1SlpVolt, .access = Access::OtpCfg},
    {.address = kSw1Ctrl, .access = Access::OtpCfg},
    {.address = kSw1Ctrl1, .access = Access::OtpCfg},
    {.address = kSw2Volt, .access = Access::OtpCfg},
    {.address = kSw2StbyVolt, .access = Access::OtpCfg},
    {.address = kSw2SlpVolt, .access = Access::OtpCfg},
    {.address = kSw2Ctrl, .access = Access::OtpCfg},
    {.address = kSw2Ctrl1, .access = Access::OtpCfg},
    {.address = kSw3Volt, .access = Access::Never},
    {.address = kSw3StbyVolt, .access = Access::Never},
    {.address = kSw3SlpVolt, .access = Access::Never},
    {.address = kSw3Ctrl, .access = Access::OtpCfg},
    {.address = kSw3Ctrl1, .access = Access::OtpCfg},
    {.address = kVsnvsCtrl, .access = Access::Cfg},
    {.address = kVrefddrCtrl, .access = Access::OtpCfg},
    {.address = kLdo1Volt, .access = Access::OtpCfg},
    {.address = kLdo1Ctrl, .access = Access::OtpCfg},
    {.address = kLdo2Volt, .access = Access::OtpCfg},
    {.address = kLdo2Ctrl, .access = Access::OtpCfg},
    {.address = kLdo3Volt, .access = Access::OtpCfg},
    {.address = kLdo3Ctrl, .access = Access::OtpCfg},
    {.address = kPwrCtrl0, .access = Access::OtpCfg},
    {.address = kPwrCtrl1, .access = Access::Cfg},
    {.address = kPwrCtrl2, .access = Access::OtpCfg},
    {.address = kPwrCtrl3, .access = Access::Command},
    {.address = kSw1PwrdnSeq, .access = Access::OtpCfg},
    {.address = kSw2PwrdnSeq, .access = Access::OtpCfg},
    {.address = kSw3PwrdnSeq, .access = Access::OtpCfg},
    {.address = kLdo1PwrdnSeq, .access = Access::OtpCfg},
    {.address = kLdo2PwrdnSeq, .access = Access::OtpCfg},
    {.address = kLdo3PwrdnSeq, .access = Access::OtpCfg},
    {.address = kVrefddrPwrdnSeq, .access = Access::OtpCfg},
    {.address = kStateInfo, .access = Access::ReadOnly},
    {.address = kI2cAddr, .access = Access::ReadOnly},
    {.address = kIoDrv0, .access = Access::Never},
    {.address = kIoDrv1, .access = Access::Never},
    {.address = kRc16Mhz, .access = Access::Never},
    {.address = kKey1, .access = Access::Never},
    {.address = kChgInt, .access = Access::Status},
    {.address = kChgIntMask, .access = Access::Mask},
    {.address = kChgIntOk, .access = Access::ReadOnly},
    {.address = kVbusSns, .access = Access::ReadOnly},
    {.address = kChgSns, .access = Access::ReadOnly},
    {.address = kBattSns, .access = Access::ReadOnly},
    {.address = kChgOper, .access = Access::OtpCfg},
    {.address = kChgTmr, .access = Access::OtpCfg},
    {.address = kChgEocCnfg, .access = Access::OtpCfg},
    {.address = kChgCurrCnfg, .access = Access::OtpCfg},
    {.address = kBattReg, .access = Access::OtpCfg},
    {.address = kBatfetCnfg, .access = Access::Cfg},
    {.address = kThmRegCnfg, .access = Access::OtpCfg},
    {.address = kVbusInlimCnfg, .access = Access::OtpCfg},
    {.address = kVbusLinDpm, .access = Access::OtpCfg},
    {.address = kUsbPhyLdoCnfg, .access = Access::Cfg},
    {.address = kDbncDelayTime, .access = Access::OtpCfg},
    {.address = kChgIntCnfg, .access = Access::OtpCfg},
    {.address = kThmAdjSetting, .access = Access::OtpCfg},
    {.address = kVbus2SysCnfg, .access = Access::OtpCfg},
    {.address = kLedPwm, .access = Access::Cfg},
    {.address = kFaultBatfetCnfg, .access = Access::OtpCfg},
    {.address = kLedCnfg, .access = Access::Cfg},
    {.address = kChgrKey2, .access = Access::Never},
}};

/**
 * @brief The access class of a register.
 * @param address An absolute register address.
 * @return The class, or `Access::Never` for an address that has no row (an undocumented register
 *         is never written).
 */
constexpr Access AccessOf(uint8_t address) noexcept {
  for (const RegisterInfo& info : kRegisters) {
    if (info.address == address) {
      return info.access;
    }
  }
  return Access::Never;
}

/**
 * @brief Whether the data sheet documents a register at this address.
 * @param address An absolute register address.
 * @return true when `kRegisters` has a row for it.
 */
constexpr bool IsDocumented(uint8_t address) noexcept {
  bool found = false;
  for (const RegisterInfo& info : kRegisters) {
    found = found || (info.address == address);
  }
  return found;
}

/**
 * @brief Whether any driver call may write a register: false for `ReadOnly`, `Never` and
 * undocumented addresses.
 * @param address An absolute register address.
 * @return true for the classes `Status`, `Mask`, `OtpCfg`, `Cfg` and `Command`.
 */
constexpr bool IsWritable(uint8_t address) noexcept {
  const Access access = AccessOf(address);
  return (access != Access::ReadOnly) && (access != Access::Never);
}

/// The charger registers on the charger page and the PMIC registers on the PMIC page agree with the
/// table above; the table has one row per documented register.
static_assert(kRegisters.size() == 90U);
static_assert(AccessOf(kSw3Volt) == Access::Never);
static_assert(!IsWritable(kDeviceId) && !IsWritable(kKey1) && !IsWritable(kChgrKey2));
/// An address in the gap between the PMIC and the charger pages: documented nowhere.
inline constexpr uint8_t kGapProbe = 0x7F;
static_assert(!IsWritable(kGapProbe) && !IsDocumented(kGapProbe));
static_assert(IsWritable(kChgOper) && IsWritable(kChgInt) && IsWritable(kPwrCtrl3));

/** @} */

} // namespace pf1550::reg

#endif // PF1550_REGISTERS_HPP
