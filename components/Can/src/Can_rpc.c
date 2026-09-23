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

#include "Can_rpc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET can_rpc_sock_t;
#define CAN_RPC_INVALID_SOCK INVALID_SOCKET
#define can_rpc_close(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
typedef int can_rpc_sock_t;
#define CAN_RPC_INVALID_SOCK (-1)
#define can_rpc_close(s) close(s)
#endif

#define CAN_RPC_REQ_MAX 16384u

static can_rpc_sock_t g_sock = CAN_RPC_INVALID_SOCK;
static uint32 g_rpc_id = 1u;
static boolean g_wsa_ready = FALSE;
static boolean g_io_failed = FALSE;
static boolean g_attempt_valid = FALSE;
static uint32 g_last_attempt_ms = 0u;

static uint32 rpc_now_ms(void)
{
#ifdef _WIN32
	return (uint32)GetTickCount();
#else
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0u;
	}
	return (uint32)((ts.tv_sec * 1000u) + (ts.tv_nsec / 1000000u));
#endif
}

static void rpc_fail_io(void)
{
	Can_RpcDisconnect();
	g_io_failed = TRUE;
}

static const char *rpc_cstr(const char *s)
{
	return (s != NULL) ? s : "";
}

static Std_ReturnType rpc_wsa_init(void)
{
#ifdef _WIN32
	WSADATA wsa;
	if (g_wsa_ready == TRUE) {
		return E_OK;
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		return E_NOT_OK;
	}
	g_wsa_ready = TRUE;
#endif
	return E_OK;
}

static Std_ReturnType rpc_connect_tcp(const char *host, uint16 port)
{
	struct sockaddr_in addr;
	int one = 1;

	if (rpc_wsa_init() != E_OK) {
		return E_NOT_OK;
	}
	g_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (g_sock == CAN_RPC_INVALID_SOCK) {
		return E_NOT_OK;
	}
	(void)setsockopt(g_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, rpc_cstr(host), &addr.sin_addr) != 1) {
		can_rpc_close(g_sock);
		g_sock = CAN_RPC_INVALID_SOCK;
		return E_NOT_OK;
	}
	if (connect(g_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		can_rpc_close(g_sock);
		g_sock = CAN_RPC_INVALID_SOCK;
		return E_NOT_OK;
	}
	return E_OK;
}

#ifndef _WIN32
static Std_ReturnType rpc_connect_unix(const char *path)
{
	struct sockaddr_un addr;

	if ((path == NULL) || (path[0] == '\0')) {
		return E_NOT_OK;
	}
	g_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (g_sock == CAN_RPC_INVALID_SOCK) {
		return E_NOT_OK;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	(void)snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	if (connect(g_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		can_rpc_close(g_sock);
		g_sock = CAN_RPC_INVALID_SOCK;
		return E_NOT_OK;
	}
	return E_OK;
}
#endif

Std_ReturnType Can_RpcConnect(const Can_RpcConfigType *rpc)
{
	if (rpc == NULL) {
		return E_NOT_OK;
	}
	Can_RpcDisconnect();

#ifndef _WIN32
	if ((rpc->socket_path != NULL) && (rpc->socket_path[0] != '\0')) {
		return rpc_connect_unix(rpc->socket_path);
	}
#endif
	return rpc_connect_tcp(rpc->host, rpc->port);
}

void Can_RpcDisconnect(void)
{
	if (g_sock != CAN_RPC_INVALID_SOCK) {
		can_rpc_close(g_sock);
		g_sock = CAN_RPC_INVALID_SOCK;
	}
}

boolean Can_RpcIsConnected(void)
{
	return (g_sock != CAN_RPC_INVALID_SOCK) ? TRUE : FALSE;
}

boolean Can_RpcTakeIoFailed(void)
{
	boolean failed = g_io_failed;
	g_io_failed = FALSE;
	return failed;
}

void Can_RpcStampAttempt(void)
{
	g_last_attempt_ms = rpc_now_ms();
	g_attempt_valid = TRUE;
}

void Can_RpcClearAttempt(void)
{
	g_attempt_valid = FALSE;
	g_io_failed = FALSE;
}

boolean Can_RpcReconnectDue(uint32 interval_ms)
{
	uint32 now;
	if (g_attempt_valid == FALSE) {
		return TRUE;
	}
	now = rpc_now_ms();
	return ((now - g_last_attempt_ms) >= interval_ms) ? TRUE : FALSE;
}

Std_ReturnType Can_RpcCall(const char *method, const char *params_json, char *resp, uint32 resp_sz)
{
	char req[CAN_RPC_REQ_MAX];
	int n;
	uint32 off = 0u;

	if ((method == NULL) || (params_json == NULL) || (resp == NULL) || (resp_sz < 2u)) {
		return E_NOT_OK;
	}
	if (g_sock == CAN_RPC_INVALID_SOCK) {
		g_io_failed = TRUE;
		return E_NOT_OK;
	}
	n = snprintf(req, sizeof(req), "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":%u}\n", method,
		     params_json, (unsigned)g_rpc_id++);
	if ((n < 0) || ((size_t)n >= sizeof(req))) {
		return E_NOT_OK;
	}
	if ((strcmp(method, "Can.Init") == 0) || (strcmp(method, "Can.Write") == 0) ||
	    (strcmp(method, "Can.SetControllerMode") == 0)) {
		(void)printf("[Can.Rpc] >> %s", req);
	}
#ifdef _WIN32
	if (send(g_sock, req, n, 0) != n) {
		rpc_fail_io();
		return E_NOT_OK;
	}
#else
	if (write(g_sock, req, (size_t)n) != (ssize_t)n) {
		rpc_fail_io();
		return E_NOT_OK;
	}
#endif
	resp[0] = '\0';
	while (off + 1u < resp_sz) {
		int r;
#ifdef _WIN32
		r = recv(g_sock, resp + off, (int)(resp_sz - off - 1u), 0);
#else
		r = (int)read(g_sock, resp + off, resp_sz - off - 1u);
#endif
		if (r <= 0) {
			rpc_fail_io();
			return E_NOT_OK;
		}
		off += (uint32)r;
		resp[off] = '\0';
		if (strchr(resp, '\n') != NULL) {
			if ((strcmp(method, "Can.Init") == 0) || (strcmp(method, "Can.Write") == 0) ||
			    (strcmp(method, "Can.SetControllerMode") == 0)) {
				(void)printf("[Can.Rpc] << %s", resp);
			}
			return E_OK;
		}
	}
	return E_NOT_OK;
}

static const char *json_find_key(const char *json, const char *key)
{
	char pat[64];
	const char *p;

	if ((json == NULL) || (key == NULL)) {
		return NULL;
	}
	(void)snprintf(pat, sizeof(pat), "\"%s\"", key);
	p = strstr(json, pat);
	if (p == NULL) {
		return NULL;
	}
	p = strchr(p + strlen(pat), ':');
	if (p == NULL) {
		return NULL;
	}
	p++;
	while ((*p != '\0') && isspace((unsigned char)*p)) {
		p++;
	}
	return p;
}

static boolean json_read_u32(const char *json, const char *key, uint32 *out)
{
	const char *p = json_find_key(json, key);
	char *end = NULL;
	unsigned long v;

	if ((p == NULL) || (out == NULL)) {
		return FALSE;
	}
	if (*p == '"') {
		p++;
	}
	v = strtoul(p, &end, 0);
	if (end == p) {
		return FALSE;
	}
	*out = (uint32)v;
	return TRUE;
}

static uint8 json_read_u8_array(const char *start, uint8 *out, uint8 max_n)
{
	const char *p = start;
	uint8 n = 0u;

	if ((p == NULL) || (out == NULL) || (*p != '[')) {
		return 0u;
	}
	p++;
	while ((*p != '\0') && (*p != ']') && (n < max_n)) {
		char *end = NULL;
		unsigned long v;
		while ((*p != '\0') && (isspace((unsigned char)*p) || (*p == ','))) {
			p++;
		}
		if (*p == ']') {
			break;
		}
		v = strtoul(p, &end, 0);
		if (end == p) {
			break;
		}
		out[n++] = (uint8)v;
		p = end;
	}
	return n;
}

static const char *object_type_str(Can_ObjectTypeType t)
{
	return (t == CAN_OBJECT_TRANSMIT) ? "TRANSMIT" : "RECEIVE";
}

static const char *handle_type_str(Can_HandleTypeType t)
{
	return (t == CAN_HANDLE_FULL) ? "FULL" : "BASIC";
}

static const char *id_type_str(Can_IdTypeType t)
{
	if (t == CAN_ID_EXTENDED) {
		return "EXTENDED";
	}
	if (t == CAN_ID_MIXED) {
		return "MIXED";
	}
	return "STANDARD";
}

static uint8 handle_of_controller_id(const Can_ConfigType *config, uint8 controller_id)
{
	uint8 i;
	if ((config == NULL) || (config->controllers == NULL)) {
		return controller_id;
	}
	for (i = 0u; i < config->controller_count; i++) {
		if (config->controllers[i].controller_id == controller_id) {
			return config->controllers[i].handle;
		}
	}
	return controller_id;
}

Std_ReturnType Can_RpcInitFromConfig(const Can_ConfigType *config)
{
	char params[CAN_RPC_REQ_MAX];
	char resp[CAN_RPC_RESP_MAX];
	uint32 off = 0u;
	uint8 i;
	uint16 h;
	int n;

	if ((config == NULL) || (config->controllers == NULL)) {
		return E_NOT_OK;
	}

	n = snprintf(params, sizeof(params), "{\"controllers\":[");
	if ((n < 0) || ((size_t)n >= sizeof(params))) {
		return E_NOT_OK;
	}
	off = (uint32)n;
	for (i = 0u; i < config->controller_count; i++) {
		const Can_ControllerConfigType *c = &config->controllers[i];
		if ((c->canfd == TRUE) && (c->fd_bitrate_bps > 0u)) {
			n = snprintf(params + off, sizeof(params) - off,
				     "%s{\"vendor\":\"simulate\",\"handle\":%u,\"name\":\"%s\",\"canfd\":true,"
				     "\"bitrate\":{\"freq\":%u},\"bitratefd\":{\"freq\":%u}}",
				     (i == 0u) ? "" : ",", (unsigned)c->handle, rpc_cstr(c->hardware_name),
				     (unsigned)c->bitrate_bps, (unsigned)c->fd_bitrate_bps);
		} else {
			n = snprintf(params + off, sizeof(params) - off,
				     "%s{\"vendor\":\"simulate\",\"handle\":%u,\"name\":\"%s\",\"canfd\":%s,"
				     "\"bitrate\":{\"freq\":%u}}",
				     (i == 0u) ? "" : ",", (unsigned)c->handle, rpc_cstr(c->hardware_name),
				     (c->canfd == TRUE) ? "true" : "false", (unsigned)c->bitrate_bps);
		}
		if ((n < 0) || ((off + (uint32)n) >= sizeof(params))) {
			return E_NOT_OK;
		}
		off += (uint32)n;
	}
	n = snprintf(params + off, sizeof(params) - off, "],\"hardwareObjects\":[");
	if ((n < 0) || ((off + (uint32)n) >= sizeof(params))) {
		return E_NOT_OK;
	}
	off += (uint32)n;
	for (h = 0u; h < config->hoh_count; h++) {
		const Can_HardwareObjectConfigType *o = &config->hohs[h];
		if (o->object_type == CAN_OBJECT_RECEIVE) {
			n = snprintf(params + off, sizeof(params) - off,
				     "%s{\"hohId\":%u,\"controllerId\":%u,\"objectType\":\"%s\",\"handleType\":\"%s\","
				     "\"idType\":\"%s\",\"canId\":%u,\"idMask\":%u}",
				     (h == 0u) ? "" : ",", (unsigned)o->hoh_id,
				     (unsigned)handle_of_controller_id(config, o->controller_id),
				     object_type_str(o->object_type), handle_type_str(o->handle_type),
				     id_type_str(o->id_type), (unsigned)o->filter_code, (unsigned)o->filter_mask);
		} else {
			n = snprintf(params + off, sizeof(params) - off,
				     "%s{\"hohId\":%u,\"controllerId\":%u,\"objectType\":\"%s\",\"handleType\":\"%s\","
				     "\"idType\":\"%s\"}",
				     (h == 0u) ? "" : ",", (unsigned)o->hoh_id,
				     (unsigned)handle_of_controller_id(config, o->controller_id),
				     object_type_str(o->object_type), handle_type_str(o->handle_type),
				     id_type_str(o->id_type));
		}
		if ((n < 0) || ((off + (uint32)n) >= sizeof(params))) {
			return E_NOT_OK;
		}
		off += (uint32)n;
	}
	n = snprintf(params + off, sizeof(params) - off, "]}");
	if ((n < 0) || ((off + (uint32)n) >= sizeof(params))) {
		return E_NOT_OK;
	}
	if (Can_RpcCall("Can.Init", params, resp, sizeof(resp)) != E_OK) {
		return E_NOT_OK;
	}
	if (strstr(resp, "\"error\"") != NULL) {
		return E_NOT_OK;
	}
	return E_OK;
}

Std_ReturnType Can_RpcParseResultCode(const char *resp, Std_ReturnType *result)
{
	uint32 code = E_NOT_OK;

	if ((resp == NULL) || (result == NULL)) {
		return E_NOT_OK;
	}
	if (strstr(resp, "\"error\"") != NULL) {
		*result = E_NOT_OK;
		return E_NOT_OK;
	}
	if (json_read_u32(resp, "resultCode", &code) != TRUE) {
		if (strstr(resp, "\"E_OK\"") != NULL) {
			code = E_OK;
		} else if (strstr(resp, "CAN_BUSY") != NULL) {
			code = CAN_BUSY;
		} else {
			code = E_NOT_OK;
		}
	}
	*result = (Std_ReturnType)code;
	return E_OK;
}

static uint16 parse_object_array(const char *resp, const char *key, void *out, uint16 max_n, uint16 elem_kind)
{
	const char *arr;
	const char *p;
	uint16 n = 0u;

	arr = json_find_key(resp, key);
	if ((arr == NULL) || (*arr != '[')) {
		return 0u;
	}
	p = arr;
	while ((p != NULL) && (n < max_n)) {
		const char *obj = strchr(p, '{');
		const char *end;
		char tmp[512];
		size_t len;
		uint32 v;

		if (obj == NULL) {
			break;
		}
		end = strchr(obj, '}');
		if (end == NULL) {
			break;
		}
		len = (size_t)(end - obj + 1);
		if (len >= sizeof(tmp)) {
			len = sizeof(tmp) - 1u;
		}
		memcpy(tmp, obj, len);
		tmp[len] = '\0';

		if (elem_kind == 0u) {
			Can_RpcIndicationType *ind = &((Can_RpcIndicationType *)out)[n];
			memset(ind, 0, sizeof(*ind));
			if (json_read_u32(tmp, "hrh", &v) == TRUE) {
				ind->hrh = (uint16)v;
			}
			if (json_read_u32(tmp, "controllerId", &v) == TRUE) {
				ind->controller_id = (uint8)v;
			}
			if (json_read_u32(tmp, "id", &v) == TRUE) {
				ind->can_id = v;
			}
			if (strstr(tmp, "\"idType\":\"EXTENDED\"") != NULL) {
				ind->extended = TRUE;
			}
			if (strstr(tmp, "\"canfd\":true") != NULL) {
				ind->canfd = TRUE;
			}
			if (json_read_u32(tmp, "dlc", &v) == TRUE) {
				ind->dlc = (uint8)v;
			} else if (json_read_u32(tmp, "length", &v) == TRUE) {
				ind->dlc = (uint8)v;
			}
			{
				const char *sdu = json_find_key(tmp, "sdu");
				if (sdu == NULL) {
					sdu = json_find_key(tmp, "data");
				}
				if (sdu != NULL) {
					uint8 got = json_read_u8_array(sdu, ind->sdu, CAN_RPC_SDU_MAX);
					if (got > 0u) {
						ind->dlc = got;
					}
				}
			}
			n++;
		} else if (elem_kind == 1u) {
			Can_RpcConfirmationType *cnf = &((Can_RpcConfirmationType *)out)[n];
			memset(cnf, 0, sizeof(*cnf));
			if (json_read_u32(tmp, "hth", &v) == TRUE) {
				cnf->hth = (uint16)v;
			}
			if (json_read_u32(tmp, "swPduHandle", &v) == TRUE) {
				cnf->sw_pdu_handle = (PduIdType)v;
			}
			n++;
		} else if (elem_kind == 2u) {
			Can_RpcBusOffType *bo = &((Can_RpcBusOffType *)out)[n];
			memset(bo, 0, sizeof(*bo));
			if (json_read_u32(tmp, "controllerId", &v) == TRUE) {
				bo->controller_id = (uint8)v;
			} else if (json_read_u32(tmp, "controller", &v) == TRUE) {
				bo->controller_id = (uint8)v;
			}
			n++;
		} else {
			Can_RpcModeIndicationType *mi = &((Can_RpcModeIndicationType *)out)[n];
			memset(mi, 0, sizeof(*mi));
			if (json_read_u32(tmp, "controllerId", &v) == TRUE) {
				mi->controller_id = (uint8)v;
			} else if (json_read_u32(tmp, "controller", &v) == TRUE) {
				mi->controller_id = (uint8)v;
			}
			if (strstr(tmp, "CAN_CS_STARTED") != NULL) {
				mi->mode = CAN_CS_STARTED;
			} else if (strstr(tmp, "CAN_CS_SLEEP") != NULL) {
				mi->mode = CAN_CS_SLEEP;
			} else if (strstr(tmp, "CAN_CS_STOPPED") != NULL) {
				mi->mode = CAN_CS_STOPPED;
			} else {
				mi->mode = CAN_CS_UNINIT;
			}
			n++;
		}
		p = end + 1;
	}
	return n;
}

uint16 Can_RpcParseIndications(const char *resp, Can_RpcIndicationType *out, uint16 max_n)
{
	return parse_object_array(resp, "indications", out, max_n, 0u);
}

uint16 Can_RpcParseConfirmations(const char *resp, Can_RpcConfirmationType *out, uint16 max_n)
{
	return parse_object_array(resp, "confirmations", out, max_n, 1u);
}

uint16 Can_RpcParseBusOffs(const char *resp, Can_RpcBusOffType *out, uint16 max_n)
{
	uint16 n = parse_object_array(resp, "busOffs", out, max_n, 2u);
	if (n == 0u) {
		n = parse_object_array(resp, "events", out, max_n, 2u);
	}
	return n;
}

uint16 Can_RpcParseModeIndications(const char *resp, Can_RpcModeIndicationType *out, uint16 max_n)
{
	return parse_object_array(resp, "indications", out, max_n, 3u);
}

Std_ReturnType Can_RpcParseControllerMode(const char *resp, Can_ControllerStateType *mode)
{
	if ((resp == NULL) || (mode == NULL)) {
		return E_NOT_OK;
	}
	if (strstr(resp, "CAN_CS_STARTED") != NULL) {
		*mode = CAN_CS_STARTED;
		return E_OK;
	}
	if (strstr(resp, "CAN_CS_SLEEP") != NULL) {
		*mode = CAN_CS_SLEEP;
		return E_OK;
	}
	if (strstr(resp, "CAN_CS_STOPPED") != NULL) {
		*mode = CAN_CS_STOPPED;
		return E_OK;
	}
	if (strstr(resp, "CAN_CS_UNINIT") != NULL) {
		*mode = CAN_CS_UNINIT;
		return E_OK;
	}
	return E_NOT_OK;
}
