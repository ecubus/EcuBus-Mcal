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

#ifndef CAN_RPC_H
#define CAN_RPC_H

#include "Can.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_RPC_DEFAULT_HOST "127.0.0.1"
#define CAN_RPC_DEFAULT_PORT 17320u

#define CAN_RPC_RESP_MAX     8192u
#define CAN_RPC_SDU_MAX      64u
#define CAN_RPC_EVENT_MAX    32u

typedef struct {
	uint16 hrh;
	uint8 controller_id;
	uint32 can_id;
	uint8 dlc;
	boolean extended;
	boolean canfd;
	uint8 sdu[CAN_RPC_SDU_MAX];
} Can_RpcIndicationType;

typedef struct {
	uint16 hth;
	PduIdType sw_pdu_handle;
} Can_RpcConfirmationType;

typedef struct {
	uint8 controller_id;
} Can_RpcBusOffType;

typedef struct {
	uint8 controller_id;
	Can_ControllerStateType mode;
} Can_RpcModeIndicationType;

Std_ReturnType Can_RpcConnect(const Can_RpcConfigType *rpc);
void Can_RpcDisconnect(void);
boolean Can_RpcIsConnected(void);
boolean Can_RpcTakeIoFailed(void);
void Can_RpcStampAttempt(void);
void Can_RpcClearAttempt(void);
boolean Can_RpcReconnectDue(uint32 interval_ms);
Std_ReturnType Can_RpcCall(const char *method, const char *params_json, char *resp, uint32 resp_sz);
Std_ReturnType Can_RpcInitFromConfig(const Can_ConfigType *config);
Std_ReturnType Can_RpcParseResultCode(const char *resp, Std_ReturnType *result);
uint16 Can_RpcParseIndications(const char *resp, Can_RpcIndicationType *out, uint16 max_n);
uint16 Can_RpcParseConfirmations(const char *resp, Can_RpcConfirmationType *out, uint16 max_n);
uint16 Can_RpcParseBusOffs(const char *resp, Can_RpcBusOffType *out, uint16 max_n);
uint16 Can_RpcParseModeIndications(const char *resp, Can_RpcModeIndicationType *out, uint16 max_n);
Std_ReturnType Can_RpcParseControllerMode(const char *resp, Can_ControllerStateType *mode);

#ifdef __cplusplus
}
#endif

#endif /* CAN_RPC_H */
