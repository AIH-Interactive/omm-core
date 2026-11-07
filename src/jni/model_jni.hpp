#pragma once

#include <jni.h>

namespace omm::jni {

// Missing methods are optional for older bridge JARs, as for the parser set.
void register_model_methods(JNIEnv *env, jclass api_class);

} // namespace omm::jni
