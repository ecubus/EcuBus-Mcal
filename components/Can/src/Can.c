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

#include "Can_internal.h"

#include <stdio.h>
#include <string.h>

extern void CanIf_TxConfirmation(PduIdType CanTxPduId);
extern void CanIf_RxIndication(Can_HwType *Mailbox, PduInfoType *PduInfoPtr);
extern void CanIf_ControllerModeIndication(uint8 ControllerId, Can_ControllerStateType ControllerMode);
extern void CanIf_ControllerBusOff(uint8 ControllerId);

#define CAN_MODE_SLOTS 64u

static Can_DriverStateType g_driver_state = CAN_UNINIT;
static const Can_ConfigType *g_config = NULL;
static boolean g_link_down = FALSE;
static boolean g_reconnecting = FALSE;
static Can_ControllerStateType g_last_mode[CAN_MODE_SLOTS];

static void modes_reset(void)
{
	uint8 i;
	for (i = 0u; i < CAN_MODE_SLOTS; i++) {
		g_last_mode[i] = CAN_CS_STOPPED;
	}
}

static void mode_store(uint8 controller, Can_ControllerStateType mode)
{
	uint8 i;
	if ((g_config == NULL) || (g_config->controllers == NULL)) {
		return;
	}
	for (i = 0u; (i < g_config->controller_count) && (i < CAN_MODE_SLOTS); i++) {
		if (g_config->controllers[i].controller_id == controller) {
			g_last_mode[i] = mode;
			return;
		}
	}
}

static const Can_ControllerConfigType *controller_cfg(uint8 controller)
{
	uint8 i;
	if ((g_config == NULL) || (g_config->controllers == NULL)) {
		return NULL;
	}
	for (i = 0u; i < g_config->controller_count; i++) {
		if (g_config->controllers[i].controller_id == controller) {
			return &g_config->controllers[i];
		}
	}
	if (controller < g_config->controller_count) {
		return &g_config->controllers[controller];
	}
	return NULL;
}

static boolean controller_valid(uint8 controller)
{
	return (controller_cfg(controller) != NULL) ? TRUE : FALSE;
}

static uint8 rpc_handle(uint8 controller)
{
	const Can_ControllerConfigType *c = controller_cfg(controller);
	return (c != NULL) ? c->handle : controller;
}

static uint8 autosar_controller_id(uint8 handle)
{
	uint8 i;
	if ((g_config == NULL) || (g_config->controllers == NULL)) {
		return handle;
	}
	for (i = 0u; i < g_config->controller_count; i++) {
		if (g_config->controllers[i].handle == handle) {
			return g_config->controllers[i].controller_id;
		}
	}
	return handle;
}

static boolean hth_valid(Can_HwHandleType hth)
{
	uint16 i;
	if ((g_config == NULL) || (g_config->hohs == NULL)) {
		return FALSE;
	}
	for (i = 0u; i < g_config->hoh_count; i++) {
		if ((g_config->hohs[i].hoh_id == hth) && (g_config->hohs[i].object_type == CAN_OBJECT_TRANSMIT)) {
			return TRUE;
		}
	}
	return FALSE;
}

void Can_Init(const Can_ConfigType *Config)
{
	if ((Config == NULL) || (Config->rpc == NULL)) {
		return;
	}
	if (g_driver_state != CAN_UNINIT) {
		return;
	}
	if (Can_RpcConnect(Config->rpc) != E_OK) {
		return;
	}
	if (Can_RpcInitFromConfig(Config) != E_OK) {
		(void)printf("[Can] Can.Init RPC failed\n");
		Can_RpcDisconnect();
		return;
	}
	g_config = Config;
	g_driver_state = CAN_READY;
	g_link_down = FALSE;
	g_reconnecting = FALSE;
	modes_reset();
	Can_RpcClearAttempt();
	(void)printf("[Can] Can_Init ready controllers=%u hohs=%u handle0=%u\n", (unsigned)Config->controller_count,
		     (unsigned)Config->hoh_count,
		     (unsigned)((Config->controller_count > 0u) ? Config->controllers[0].handle : 0u));
}

void Can_DeInit(void)
{
	char resp[CAN_RPC_RESP_MAX];
	if ((g_driver_state == CAN_READY) && (g_link_down == FALSE) && (Can_RpcIsConnected() == TRUE)) {
		(void)Can_RpcCall("Can.DeInit", "{}", resp, sizeof(resp));
		(void)Can_RpcTakeIoFailed();
	}
	Can_RpcDisconnect();
	g_config = NULL;
	g_driver_state = CAN_UNINIT;
	g_link_down = FALSE;
	g_reconnecting = FALSE;
	Can_RpcClearAttempt();
}

static const char *transition_str(Can_ControllerStateType state)
{
	switch (state) {
	case CAN_CS_STARTED:
		return "CAN_T_START";
	case CAN_CS_STOPPED:
		return "CAN_T_STOP";
	case CAN_CS_SLEEP:
		return "CAN_T_SLEEP";
	default:
		return NULL;
	}
}

static Std_ReturnType can_restore_modes(void)
{
	uint8 i;
	if ((g_config == NULL) || (g_config->controllers == NULL)) {
		return E_NOT_OK;
	}
	for (i = 0u; (i < g_config->controller_count) && (i < CAN_MODE_SLOTS); i++) {
		Can_ControllerStateType mode = g_last_mode[i];
		const char *tr;
		char params[128];
		char resp[CAN_RPC_RESP_MAX];
		int n;
		if ((mode != CAN_CS_STARTED) && (mode != CAN_CS_SLEEP)) {
			continue;
		}
		tr = transition_str(mode);
		if (tr == NULL) {
			continue;
		}
		n = snprintf(params, sizeof(params), "{\"controller\":%u,\"transition\":\"%s\"}",
			     (unsigned)g_config->controllers[i].handle, tr);
		if ((n < 0) || ((size_t)n >= sizeof(params))) {
			return E_NOT_OK;
		}
		if (Can_RpcCall("Can.SetControllerMode", params, resp, sizeof(resp)) != E_OK) {
			(void)Can_RpcTakeIoFailed();
			return E_NOT_OK;
		}
	}
	return E_OK;
}

static Std_ReturnType can_try_reconnect(void)
{
	const Can_RpcConfigType *rpc;
	if ((g_config == NULL) || (g_config->rpc == NULL) || (g_reconnecting == TRUE)) {
		return E_NOT_OK;
	}
	rpc = g_config->rpc;
	if (rpc->auto_reconnect != TRUE) {
		return E_NOT_OK;
	}
	if (Can_RpcReconnectDue(rpc->reconnect_interval_ms) != TRUE) {
		return E_NOT_OK;
	}
	g_reconnecting = TRUE;
	Can_RpcStampAttempt();
	if (Can_RpcConnect(rpc) != E_OK) {
		g_reconnecting = FALSE;
		(void)printf("[Can] reconnect failed\n");
		return E_NOT_OK;
	}
	if ((Can_RpcInitFromConfig(g_config) != E_OK) || (can_restore_modes() != E_OK)) {
		(void)Can_RpcTakeIoFailed();
		Can_RpcDisconnect();
		g_reconnecting = FALSE;
		(void)printf("[Can] reconnect init failed\n");
		return E_NOT_OK;
	}
	(void)Can_RpcTakeIoFailed();
	g_link_down = FALSE;
	g_reconnecting = FALSE;
	(void)printf("[Can] reconnect ok\n");
	return E_OK;
}

static void can_note_link_down(void)
{
	if (g_link_down == FALSE) {
		(void)printf("[Can] RPC link down\n");
	}
	g_link_down = TRUE;
	Can_RpcDisconnect();
	Can_RpcStampAttempt();
}

static Std_ReturnType can_rpc(const char *method, const char *params_json, char *resp, uint32 resp_sz)
{
	Std_ReturnType result;
	if (g_reconnecting == TRUE) {
		return E_NOT_OK;
	}
	if (g_link_down == TRUE) {
		if (can_try_reconnect() != E_OK) {
			return E_NOT_OK;
		}
	}
	result = Can_RpcCall(method, params_json, resp, resp_sz);
	if ((result != E_OK) && (Can_RpcTakeIoFailed() == TRUE)) {
		can_note_link_down();
	}
	return result;
}

Std_ReturnType Can_SetControllerMode(uint8 Controller, Can_ControllerStateType Transition)
{
	char params[128];
	char resp[CAN_RPC_RESP_MAX];
	const char *tr;
	Std_ReturnType result = E_NOT_OK;

	if (g_driver_state != CAN_READY) {
		(void)printf("[Can] SetControllerMode while UNINIT Controller=%u\n", (unsigned)Controller);
		return E_NOT_OK;
	}
	if (controller_cfg(Controller) == NULL) {
		(void)printf("[Can] SetControllerMode invalid Controller=%u count=%u\n", (unsigned)Controller,
			     (unsigned)((g_config != NULL) ? g_config->controller_count : 0u));
		return E_NOT_OK;
	}
	tr = transition_str(Transition);
	if (tr == NULL) {
		(void)printf("[Can] SetControllerMode invalid state=%u Controller=%u\n", (unsigned)Transition,
			     (unsigned)Controller);
		return E_NOT_OK;
	}
	(void)snprintf(params, sizeof(params), "{\"controller\":%u,\"transition\":\"%s\"}", (unsigned)rpc_handle(Controller),
		       tr);
	if (can_rpc("Can.SetControllerMode", params, resp, sizeof(resp)) != E_OK) {
		(void)printf("[Can] SetControllerMode RPC call failed Controller=%u handle=%u\n", (unsigned)Controller,
			     (unsigned)rpc_handle(Controller));
		return E_NOT_OK;
	}
	(void)Can_RpcParseResultCode(resp, &result);
	(void)printf("[Can] SetControllerMode Controller=%u handle=%u %s result=%u\n", (unsigned)Controller,
		     (unsigned)rpc_handle(Controller), tr, (unsigned)result);
	if (result == E_OK) {
		mode_store(Controller, Transition);
		CanIf_ControllerModeIndication(Controller, Transition);
	}
	return result;
}

Std_ReturnType Can_GetControllerMode(uint8 Controller, Can_ControllerStateType *ControllerModePtr)
{
	char params[64];
	char resp[CAN_RPC_RESP_MAX];

	if ((g_driver_state != CAN_READY) || (ControllerModePtr == NULL) || (controller_cfg(Controller) == NULL)) {
		return E_NOT_OK;
	}
	(void)snprintf(params, sizeof(params), "{\"controller\":%u}", (unsigned)rpc_handle(Controller));
	if (can_rpc("Can.GetControllerMode", params, resp, sizeof(resp)) != E_OK) {
		return E_NOT_OK;
	}
	return Can_RpcParseControllerMode(resp, ControllerModePtr);
}

Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo)
{
	char params[512];
	char sdu_json[256];
	char resp[CAN_RPC_RESP_MAX];
	Std_ReturnType result = E_NOT_OK;
	uint32 off = 0u;
	uint8 i;
	uint32 can_id;
	const char *id_type = "STANDARD";
	const char *canfd = "false";
	int n;

	if ((g_driver_state != CAN_READY) || (PduInfo == NULL) || (PduInfo->sdu == NULL)) {
		return E_NOT_OK;
	}
	if (hth_valid(Hth) != TRUE) {
		(void)printf("[Can] Write invalid HTH %u\n", (unsigned)Hth);
		return E_NOT_OK;
	}
	if (PduInfo->length > CAN_RPC_SDU_MAX) {
		return E_NOT_OK;
	}

	can_id = PduInfo->id & CAN_ID_VALUE_MASK;
	if ((PduInfo->id & CAN_ID_EXTENDED_MASK) != 0u) {
		id_type = "EXTENDED";
	}
	if ((PduInfo->id & CAN_ID_FD_MASK) != 0u) {
		canfd = "true";
	}

	n = snprintf(sdu_json, sizeof(sdu_json), "[");
	if ((n < 0) || ((size_t)n >= sizeof(sdu_json))) {
		return E_NOT_OK;
	}
	off = (uint32)n;
	for (i = 0u; i < PduInfo->length; i++) {
		n = snprintf(sdu_json + off, sizeof(sdu_json) - off, "%s%u", (i == 0u) ? "" : ",",
			     (unsigned)PduInfo->sdu[i]);
		if ((n < 0) || ((off + (uint32)n) >= sizeof(sdu_json))) {
			return E_NOT_OK;
		}
		off += (uint32)n;
	}
	n = snprintf(sdu_json + off, sizeof(sdu_json) - off, "]");
	if ((n < 0) || ((off + (uint32)n) >= sizeof(sdu_json))) {
		return E_NOT_OK;
	}

	n = snprintf(params, sizeof(params),
		     "{\"hth\":%u,\"swPduHandle\":%u,\"id\":%u,\"sdu\":%s,\"idType\":\"%s\",\"canfd\":%s,\"brs\":false}",
		     (unsigned)Hth, (unsigned)PduInfo->swPduHandle, (unsigned)can_id, sdu_json, id_type, canfd);
	if ((n < 0) || ((size_t)n >= sizeof(params))) {
		return E_NOT_OK;
	}
	if (can_rpc("Can.Write", params, resp, sizeof(resp)) != E_OK) {
		(void)printf("[Can] Write RPC call failed hth=%u\n", (unsigned)Hth);
		return E_NOT_OK;
	}
	(void)Can_RpcParseResultCode(resp, &result);
	(void)printf("[Can] Write hth=%u id=0x%lx len=%u result=%u\n", (unsigned)Hth, (unsigned long)can_id,
		     (unsigned)PduInfo->length, (unsigned)result);
	return result;
}

void Can_DisableControllerInterrupts(uint8 Controller)
{
	char params[64];
	char resp[CAN_RPC_RESP_MAX];
	if ((g_driver_state != CAN_READY) || (controller_cfg(Controller) == NULL)) {
		return;
	}
	(void)snprintf(params, sizeof(params), "{\"controller\":%u}", (unsigned)rpc_handle(Controller));
	(void)can_rpc("Can.DisableControllerInterrupts", params, resp, sizeof(resp));
}

void Can_EnableControllerInterrupts(uint8 Controller)
{
	char params[64];
	char resp[CAN_RPC_RESP_MAX];
	if ((g_driver_state != CAN_READY) || (controller_cfg(Controller) == NULL)) {
		return;
	}
	(void)snprintf(params, sizeof(params), "{\"controller\":%u}", (unsigned)rpc_handle(Controller));
	(void)can_rpc("Can.EnableControllerInterrupts", params, resp, sizeof(resp));
}

Std_ReturnType Can_CheckWakeup(uint8 Controller)
{
	char params[64];
	char resp[CAN_RPC_RESP_MAX];
	Std_ReturnType result = E_NOT_OK;
	if ((g_driver_state != CAN_READY) || (controller_cfg(Controller) == NULL)) {
		return E_NOT_OK;
	}
	(void)snprintf(params, sizeof(params), "{\"controller\":%u}", (unsigned)rpc_handle(Controller));
	if (can_rpc("Can.CheckWakeup", params, resp, sizeof(resp)) != E_OK) {
		return E_NOT_OK;
	}
	(void)Can_RpcParseResultCode(resp, &result);
	return result;
}

Std_ReturnType Can_GetControllerErrorState(uint8 ControllerId, Can_ErrorStateType *ErrorStatePtr)
{
	char params[64];
	char resp[CAN_RPC_RESP_MAX];
	if ((g_driver_state != CAN_READY) || (ErrorStatePtr == NULL) || (controller_cfg(ControllerId) == NULL)) {
		return E_NOT_OK;
	}
	(void)snprintf(params, sizeof(params), "{\"controller\":%u}", (unsigned)rpc_handle(ControllerId));
	if (can_rpc("Can.GetControllerErrorState", params, resp, sizeof(resp)) != E_OK) {
		return E_NOT_OK;
	}
	if (strstr(resp, "CAN_ERRORSTATE_BUSOFF") != NULL) {
		*ErrorStatePtr = CAN_ERRORSTATE_BUSOFF;
	} else if (strstr(resp, "CAN_ERRORSTATE_PASSIVE") != NULL) {
		*ErrorStatePtr = CAN_ERRORSTATE_PASSIVE;
	} else {
		*ErrorStatePtr = CAN_ERRORSTATE_ACTIVE;
	}
	return E_OK;
}

#if (CAN_SET_BAUDRATE_API == STD_ON)
Std_ReturnType Can_SetBaudrate(uint8 Controller, uint16 BaudRateConfigID)
{
	char params[96];
	char resp[CAN_RPC_RESP_MAX];
	Std_ReturnType result = E_NOT_OK;
	if ((g_driver_state != CAN_READY) || (controller_cfg(Controller) == NULL)) {
		return E_NOT_OK;
	}
	(void)snprintf(params, sizeof(params), "{\"controller\":%u,\"baudRateConfigID\":%u}",
		       (unsigned)rpc_handle(Controller), (unsigned)BaudRateConfigID);
	if (can_rpc("Can.SetBaudrate", params, resp, sizeof(resp)) != E_OK) {
		return E_NOT_OK;
	}
	(void)Can_RpcParseResultCode(resp, &result);
	return result;
}
#endif

#if (CAN_VERSION_INFO_API == STD_ON)
void Can_GetVersionInfo(Std_VersionInfoType *versioninfo)
{
	if (versioninfo == NULL) {
		return;
	}
	versioninfo->vendorID = CAN_VENDOR_ID;
	versioninfo->moduleID = CAN_MODULE_ID;
	versioninfo->sw_major_version = CAN_SW_MAJOR_VERSION;
	versioninfo->sw_minor_version = CAN_SW_MINOR_VERSION;
	versioninfo->sw_patch_version = CAN_SW_PATCH_VERSION;
}
#endif

void Can_MainFunction_Write(void)
{
	char resp[CAN_RPC_RESP_MAX];
	Can_RpcConfirmationType events[CAN_RPC_EVENT_MAX];
	uint16 n;
	uint16 i;

	if (g_driver_state != CAN_READY) {
		return;
	}
	if (can_rpc("Can.MainFunction_Write", "{}", resp, sizeof(resp)) != E_OK) {
		return;
	}
	n = Can_RpcParseConfirmations(resp, events, CAN_RPC_EVENT_MAX);
	for (i = 0u; i < n; i++) {
		CanIf_TxConfirmation(events[i].sw_pdu_handle);
	}
}

void Can_MainFunction_Read(void)
{
	char resp[CAN_RPC_RESP_MAX];
	Can_RpcIndicationType events[CAN_RPC_EVENT_MAX];
	uint16 n;
	uint16 i;

	if (g_driver_state != CAN_READY) {
		return;
	}
	if (can_rpc("Can.MainFunction_Read", "{\"max\":16}", resp, sizeof(resp)) != E_OK) {
		return;
	}
	n = Can_RpcParseIndications(resp, events, CAN_RPC_EVENT_MAX);
	if (n > 0u) {
		(void)printf("[Can] MainFunction_Read n=%u id0=0x%lx hrh0=%u\n", (unsigned)n,
			     (unsigned long)events[0].can_id, (unsigned)events[0].hrh);
	}
	for (i = 0u; i < n; i++) {
		Can_HwType mailbox;
		PduInfoType pdu;
		mailbox.CanId = events[i].can_id & CAN_ID_VALUE_MASK;
		if (events[i].extended == TRUE) {
			mailbox.CanId |= CAN_ID_EXTENDED_MASK;
		}
		if (events[i].canfd == TRUE) {
			mailbox.CanId |= CAN_ID_FD_MASK;
		}
		mailbox.Hoh = events[i].hrh;
		mailbox.ControllerId = autosar_controller_id(events[i].controller_id);
		pdu.SduDataPtr = events[i].sdu;
		pdu.MetaDataPtr = NULL;
		pdu.SduLength = events[i].dlc;
		CanIf_RxIndication(&mailbox, &pdu);
	}
}

void Can_MainFunction_BusOff(void)
{
	char resp[CAN_RPC_RESP_MAX];
	Can_RpcBusOffType events[CAN_RPC_EVENT_MAX];
	uint16 n;
	uint16 i;

	if (g_driver_state != CAN_READY) {
		return;
	}
	if (can_rpc("Can.MainFunction_BusOff", "{}", resp, sizeof(resp)) != E_OK) {
		return;
	}
	n = Can_RpcParseBusOffs(resp, events, CAN_RPC_EVENT_MAX);
	for (i = 0u; i < n; i++) {
		CanIf_ControllerBusOff(autosar_controller_id(events[i].controller_id));
	}
}

void Can_MainFunction_Wakeup(void)
{
	char resp[CAN_RPC_RESP_MAX];
	if (g_driver_state != CAN_READY) {
		return;
	}
	(void)can_rpc("Can.MainFunction_Wakeup", "{}", resp, sizeof(resp));
}

void Can_MainFunction_Mode(void)
{
	char resp[CAN_RPC_RESP_MAX];
	Can_RpcModeIndicationType events[CAN_RPC_EVENT_MAX];
	uint16 n;
	uint16 i;

	if (g_driver_state != CAN_READY) {
		return;
	}
	if (can_rpc("Can.MainFunction_Mode", "{}", resp, sizeof(resp)) != E_OK) {
		return;
	}
	n = Can_RpcParseModeIndications(resp, events, CAN_RPC_EVENT_MAX);
	for (i = 0u; i < n; i++) {
		CanIf_ControllerModeIndication(autosar_controller_id(events[i].controller_id), events[i].mode);
	}
}
