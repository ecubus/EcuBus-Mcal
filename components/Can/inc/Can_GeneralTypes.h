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

#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H

#include "Std_Types.h"
#include "ComStack_Types.h"

typedef uint32 Can_IdType;
typedef uint16 Can_HwHandleType;

typedef enum {
	CAN_CS_UNINIT = 0U,
	CAN_CS_STARTED,
	CAN_CS_STOPPED,
	CAN_CS_SLEEP
} Can_ControllerStateType;

typedef enum {
	CAN_T_START = 0U,
	CAN_T_STOP,
	CAN_T_SLEEP,
	CAN_T_WAKEUP
} Can_StateTransitionType;

typedef enum {
	CAN_ERRORSTATE_ACTIVE = 0U,
	CAN_ERRORSTATE_PASSIVE,
	CAN_ERRORSTATE_BUSOFF
} Can_ErrorStateType;

typedef struct {
	Can_IdType CanId;
	Can_HwHandleType Hoh;
	uint8 ControllerId;
} Can_HwType;

typedef struct {
	Can_IdType id;
	PduIdType swPduHandle;
	uint8 length;
	uint8 *sdu;
} Can_PduType;

#ifndef CAN_BUSY
#define CAN_BUSY ((Std_ReturnType)2u)
#endif

#endif /* CAN_GENERAL_TYPES_H */
