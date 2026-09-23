/*
 * Copyright (c) 2025 Li Auto Inc. and its affiliates
 * Licensed under the Apache License, Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CAN_H
#define CAN_H

#include "Can_GeneralTypes.h"
#include "Can_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_SID_INIT                      (0x00u)
#define CAN_SID_MAINFUNCTION_WRITE        (0x01u)
#define CAN_SID_SET_CONTROLLER_MODE       (0x03u)
#define CAN_SID_DISABLE_CONTROLLER_INT    (0x04u)
#define CAN_SID_ENABLE_CONTROLLER_INT     (0x05u)
#define CAN_SID_WRITE                     (0x06u)
#define CAN_SID_GET_VERSION_INFO          (0x07u)
#define CAN_SID_MAINFUNCTION_READ         (0x08u)
#define CAN_SID_MAINFUNCTION_BUSOFF       (0x09u)
#define CAN_SID_MAINFUNCTION_WAKEUP       (0x0Au)
#define CAN_SID_CHECK_WAKEUP              (0x0Bu)
#define CAN_SID_MAINFUNCTION_MODE         (0x0Cu)
#define CAN_SID_SET_BAUDRATE              (0x0Fu)
#define CAN_SID_DEINIT                    (0x10u)
#define CAN_SID_GET_CONTROLLER_MODE       (0x12u)
#define CAN_SID_GET_CONTROLLER_ERRORSTATE (0x11u)

#define CAN_E_PARAM_POINTER               (0x01u)
#define CAN_E_PARAM_HANDLE                (0x02u)
#define CAN_E_PARAM_DATA_LENGTH           (0x03u)
#define CAN_E_PARAM_CONTROLLER            (0x04u)
#define CAN_E_UNINIT                      (0x05u)
#define CAN_E_TRANSITION                  (0x06u)
#define CAN_E_PARAM_BAUDRATE              (0x07u)
#define CAN_E_INIT_FAILED                 (0x09u)

#define CAN_ID_EXTENDED_MASK              (0x80000000u)
#define CAN_ID_FD_MASK                    (0x40000000u)
#define CAN_ID_VALUE_MASK                 (0x1FFFFFFFu)

typedef enum {
	CAN_OBJECT_RECEIVE = 0,
	CAN_OBJECT_TRANSMIT
} Can_ObjectTypeType;

typedef enum {
	CAN_HANDLE_BASIC = 0,
	CAN_HANDLE_FULL
} Can_HandleTypeType;

typedef enum {
	CAN_ID_STANDARD = 0,
	CAN_ID_EXTENDED,
	CAN_ID_MIXED
} Can_IdTypeType;

typedef struct {
	const char *host;
	uint16 port;
	const char *socket_path;
	boolean auto_reconnect;
	uint16 reconnect_interval_ms;
} Can_RpcConfigType;

typedef struct {
	uint8 controller_id;
	boolean activation;
	const char *name;
	const char *vendor;
	uint8 handle;
	boolean canfd;
	uint32 bitrate_bps;
	uint32 fd_bitrate_bps;
	boolean brs;
	const char *hardware_name;
	uint16 baud_id;
} Can_ControllerConfigType;

typedef struct {
	uint16 hoh_id;
	uint8 controller_id;
	Can_ObjectTypeType object_type;
	Can_HandleTypeType handle_type;
	Can_IdTypeType id_type;
	uint8 padding;
	boolean has_filter;
	uint32 filter_code;
	uint32 filter_mask;
} Can_HardwareObjectConfigType;

typedef struct {
	const Can_RpcConfigType *rpc;
	const Can_ControllerConfigType *controllers;
	uint8 controller_count;
	const Can_HardwareObjectConfigType *hohs;
	uint16 hoh_count;
} Can_ConfigType;

void Can_Init(const Can_ConfigType *Config);
void Can_DeInit(void);
Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_ControllerStateType Transition);
Std_ReturnType Can_GetControllerMode(uint8 Controller, Can_ControllerStateType *ControllerModePtr);
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);
void Can_DisableControllerInterrupts(uint8 Controller);
void Can_EnableControllerInterrupts(uint8 Controller);
Std_ReturnType Can_CheckWakeup(uint8 Controller);
Std_ReturnType Can_GetControllerErrorState(uint8 ControllerId, Can_ErrorStateType *ErrorStatePtr);
#if (CAN_SET_BAUDRATE_API == STD_ON)
Std_ReturnType Can_SetBaudrate(uint8 Controller, uint16 BaudRateConfigID);
#endif
#if (CAN_VERSION_INFO_API == STD_ON)
void Can_GetVersionInfo(Std_VersionInfoType *versioninfo);
#endif
void Can_MainFunction_Write(void);
void Can_MainFunction_Read(void);
void Can_MainFunction_BusOff(void);
void Can_MainFunction_Wakeup(void);
void Can_MainFunction_Mode(void);

#include "Can_PBcfg.h"

#ifdef __cplusplus
}
#endif

#endif /* CAN_H */
