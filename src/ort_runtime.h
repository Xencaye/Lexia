#pragma once
// The process's one ONNX Runtime environment, and the switch that keeps ONNX Runtime's telemetry off.
//
// ADR-0005 (no telemetry without the user's request): the prebuilt onnxruntime.dll writes ETW telemetry
// events that the Windows diagnostic service can send to Microsoft. The switch is process-wide (the OrtEnv is
// a process singleton and the provider's flag is global to onnxruntime.dll), so it also covers the Ort::Env that
// piper.dll creates for the Voices. No ONNX Runtime header is needed here.
#include <memory>

namespace Ort {
struct Env;  // onnxruntime_cxx_api.h declares it as a struct
}

// Makes the environment (if it is not made yet) and turns telemetry off. Call it at the start of the process and
// before any model or Voice session is created (ORT sends its first telemetry event when the first session is
// created). Safe to call again, from any thread. It never throws: a failure to make the environment is logged
// and left for the next SharedOrtEnv() call (a function-local static whose initialiser threw is run again), so
// the OCR's own start-failure path still reports it. A failure to turn telemetry off is logged as an error and
// Lexia goes on.
void StartOnnxRuntime() noexcept;

// The one environment, shared by every OcrEngine (each keeps a reference, so it outlives their sessions). The
// first call makes it and turns telemetry off. Can throw (Ort::Exception) when the environment cannot be made.
// Any thread.
std::shared_ptr<Ort::Env> SharedOrtEnv();
