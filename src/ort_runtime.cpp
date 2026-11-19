#include "ort_runtime.h"

#include <onnxruntime_cxx_api.h>

#include <exception>

#include "log.h"

// One ONNX Runtime environment for the whole process, as ORT asks: a Language switch keeps two OCR engines
// alive for a moment. Each engine holds a reference, so it outlives their sessions.
//
// The environment and the telemetry switch are made in the same one-time initialisation (a function-local
// static: run once, thread-safe), so telemetry is off before any session exists. If the environment cannot be
// made the exception leaves the initialiser, which then runs again at the next call (the standard says so), so
// a later caller sees the failure too.
std::shared_ptr<Ort::Env> SharedOrtEnv() {
    static const std::shared_ptr<Ort::Env> env = [] {
        auto e = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "lexia");
        try {
            e->DisableTelemetryEvents();
            LogInfo("ONNX Runtime telemetry is off (ADR-0005).");
        } catch (const Ort::Exception& ex) {
            // Lexia goes on (it reads and speaks fine), but this is a breach of ADR-0005: be loud.
            LogError("ONNX Runtime telemetry could not be turned off (ADR-0005): ", ex.what());
        }
        return e;
    }();
    return env;
}

void StartOnnxRuntime() noexcept {
    // Creating the environment runs the one-time set-up above; the result is not needed here (each
    // OcrEngine takes its own reference). A failure is only logged: the OCR engine's own start makes the
    // environment again and reports the failure to the person.
    try {
        (void)SharedOrtEnv();
    } catch (const std::exception& e) {
        LogError("ONNX Runtime could not start: ", e.what());
    }
}
