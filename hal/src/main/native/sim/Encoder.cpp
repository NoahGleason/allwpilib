/*----------------------------------------------------------------------------*/
/* Copyright (c) 2017 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "HAL/Encoder.h"

#include "CounterInternal.h"
#include "HAL/Counter.h"
#include "HAL/Errors.h"
#include "HAL/handles/HandlesInternal.h"
#include "HAL/handles/LimitedHandleResource.h"
#include "MockData/EncoderDataInternal.h"
#include "PortsInternal.h"

using namespace hal;

namespace {
struct Encoder {
  HAL_Handle nativeHandle;
  HAL_EncoderEncodingType encodingType;
  double distancePerPulse;
  uint8_t index;
};
struct Empty {};
}  // namespace

static LimitedHandleResource<HAL_EncoderHandle, Encoder,
                             kNumEncoders + kNumCounters,
                             HAL_HandleEnum::Encoder>* encoderHandles;

static LimitedHandleResource<HAL_FPGAEncoderHandle, Empty, kNumEncoders,
                             HAL_HandleEnum::FPGAEncoder>* fpgaEncoderHandles;

namespace hal {
namespace init {
void InitializeEncoder() {
  static LimitedHandleResource<HAL_FPGAEncoderHandle, Empty, kNumEncoders,
                               HAL_HandleEnum::FPGAEncoder>
      feH;
  fpgaEncoderHandles = &feH;
  static LimitedHandleResource<HAL_EncoderHandle, Encoder,
                               kNumEncoders + kNumCounters,
                               HAL_HandleEnum::Encoder>
      eH;
  encoderHandles = &eH;
}
}  // namespace init
}  // namespace hal

extern "C" {
HAL_EncoderHandle HAL_InitializeEncoder(
    HAL_Handle digitalSourceHandleA, HAL_AnalogTriggerType analogTriggerTypeA,
    HAL_Handle digitalSourceHandleB, HAL_AnalogTriggerType analogTriggerTypeB,
    HAL_Bool reverseDirection, HAL_EncoderEncodingType encodingType,
    int32_t* status) {
  HAL_Handle nativeHandle = HAL_kInvalidHandle;
  if (encodingType == HAL_EncoderEncodingType::HAL_Encoder_k4X) {
    // k4x, allocate encoder
    nativeHandle = fpgaEncoderHandles->Allocate();
  } else {
    // k2x or k1x, allocate counter
    nativeHandle = counterHandles->Allocate();
  }
  if (nativeHandle == HAL_kInvalidHandle) {
    *status = NO_AVAILABLE_RESOURCES;
    return HAL_kInvalidHandle;
  }
  auto handle = encoderHandles->Allocate();
  if (handle == HAL_kInvalidHandle) {
    *status = NO_AVAILABLE_RESOURCES;
    return HAL_kInvalidHandle;
  }
  auto encoder = encoderHandles->Get(handle);
  if (encoder == nullptr) {  // would only occur on thread issue
    *status = HAL_HANDLE_ERROR;
    return HAL_kInvalidHandle;
  }
  int16_t index = getHandleIndex(handle);
  SimEncoderData[index].SetInitialized(true);
  // TODO: Add encoding type to Sim data
  encoder->index = index;
  encoder->nativeHandle = nativeHandle;
  encoder->encodingType = encodingType;
  encoder->distancePerPulse = 1.0;
  return handle;
}

void HAL_FreeEncoder(HAL_EncoderHandle encoderHandle, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  encoderHandles->Free(encoderHandle);
  if (encoder == nullptr) return;
  if (isHandleType(encoder->nativeHandle, HAL_HandleEnum::FPGAEncoder)) {
    fpgaEncoderHandles->Free(encoder->nativeHandle);
  } else if (isHandleType(encoder->nativeHandle, HAL_HandleEnum::Counter)) {
    counterHandles->Free(encoder->nativeHandle);
  }
  SimEncoderData[encoder->index].SetInitialized(false);
}

static inline int EncodingScaleFactor(Encoder* encoder) {
  switch (encoder->encodingType) {
    case HAL_Encoder_k1X:
      return 1;
    case HAL_Encoder_k2X:
      return 2;
    case HAL_Encoder_k4X:
      return 4;
    default:
      return 0;
  }
}

static inline double DecodingScaleFactor(Encoder* encoder) {
  switch (encoder->encodingType) {
    case HAL_Encoder_k1X:
      return 1.0;
    case HAL_Encoder_k2X:
      return 0.5;
    case HAL_Encoder_k4X:
      return 0.25;
    default:
      return 0.0;
  }
}

int32_t HAL_GetEncoder(HAL_EncoderHandle encoderHandle, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return SimEncoderData[encoder->index].GetCount();
}
int32_t HAL_GetEncoderRaw(HAL_EncoderHandle encoderHandle, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return SimEncoderData[encoder->index].GetCount() /
         DecodingScaleFactor(encoder.get());
}
int32_t HAL_GetEncoderEncodingScale(HAL_EncoderHandle encoderHandle,
                                    int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return EncodingScaleFactor(encoder.get());
}
void HAL_ResetEncoder(HAL_EncoderHandle encoderHandle, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return;
  }

  SimEncoderData[encoder->index].SetCount(0);
  SimEncoderData[encoder->index].SetPeriod(std::numeric_limits<double>::max());
  SimEncoderData[encoder->index].SetReset(true);
}
double HAL_GetEncoderPeriod(HAL_EncoderHandle encoderHandle, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return SimEncoderData[encoder->index].GetPeriod();
}
void HAL_SetEncoderMaxPeriod(HAL_EncoderHandle encoderHandle, double maxPeriod,
                             int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return;
  }

  SimEncoderData[encoder->index].SetMaxPeriod(maxPeriod);
}
HAL_Bool HAL_GetEncoderStopped(HAL_EncoderHandle encoderHandle,
                               int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return SimEncoderData[encoder->index].GetPeriod() >
         SimEncoderData[encoder->index].GetMaxPeriod();
}
HAL_Bool HAL_GetEncoderDirection(HAL_EncoderHandle encoderHandle,
                                 int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return SimEncoderData[encoder->index].GetDirection();
}
double HAL_GetEncoderDistance(HAL_EncoderHandle encoderHandle,
                              int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return SimEncoderData[encoder->index].GetCount() * encoder->distancePerPulse;
}
double HAL_GetEncoderRate(HAL_EncoderHandle encoderHandle, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return encoder->distancePerPulse / SimEncoderData[encoder->index].GetPeriod();
}
void HAL_SetEncoderMinRate(HAL_EncoderHandle encoderHandle, double minRate,
                           int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return;
  }

  if (minRate == 0.0) {
    *status = PARAMETER_OUT_OF_RANGE;
    return;
  }

  SimEncoderData[encoder->index].SetMaxPeriod(encoder->distancePerPulse /
                                              minRate);
}
void HAL_SetEncoderDistancePerPulse(HAL_EncoderHandle encoderHandle,
                                    double distancePerPulse, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return;
  }

  if (distancePerPulse == 0.0) {
    *status = PARAMETER_OUT_OF_RANGE;
    return;
  }
  encoder->distancePerPulse = distancePerPulse;
}
void HAL_SetEncoderReverseDirection(HAL_EncoderHandle encoderHandle,
                                    HAL_Bool reverseDirection,
                                    int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return;
  }

  SimEncoderData[encoder->index].SetReverseDirection(reverseDirection);
}
void HAL_SetEncoderSamplesToAverage(HAL_EncoderHandle encoderHandle,
                                    int32_t samplesToAverage, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return;
  }

  SimEncoderData[encoder->index].SetSamplesToAverage(samplesToAverage);
}
int32_t HAL_GetEncoderSamplesToAverage(HAL_EncoderHandle encoderHandle,
                                       int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return SimEncoderData[encoder->index].GetSamplesToAverage();
}

void HAL_SetEncoderIndexSource(HAL_EncoderHandle encoderHandle,
                               HAL_Handle digitalSourceHandle,
                               HAL_AnalogTriggerType analogTriggerType,
                               HAL_EncoderIndexingType type, int32_t* status) {
  // Not implemented yet
}

int32_t HAL_GetEncoderFPGAIndex(HAL_EncoderHandle encoderHandle,
                                int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0;
  }

  return encoder->index;
}

double HAL_GetEncoderDecodingScaleFactor(HAL_EncoderHandle encoderHandle,
                                         int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0.0;
  }

  return DecodingScaleFactor(encoder.get());
}

double HAL_GetEncoderDistancePerPulse(HAL_EncoderHandle encoderHandle,
                                      int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return 0.0;
  }

  return encoder->distancePerPulse;
}

HAL_EncoderEncodingType HAL_GetEncoderEncodingType(
    HAL_EncoderHandle encoderHandle, int32_t* status) {
  auto encoder = encoderHandles->Get(encoderHandle);
  if (encoder == nullptr) {
    *status = HAL_HANDLE_ERROR;
    return HAL_Encoder_k4X;  // default to k4x
  }

  return encoder->encodingType;
}
}  // extern "C"
