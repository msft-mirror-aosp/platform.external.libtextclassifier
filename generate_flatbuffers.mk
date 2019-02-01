FLATC := $(HOST_OUT_EXECUTABLES)/flatc$(HOST_EXECUTABLE_SUFFIX)

define transform-fbs-to-cpp
@echo "Flatc: $@ <= $(PRIVATE_INPUT_FBS)"
@rm -f $@
@mkdir -p $(dir $@)
$(FLATC) \
    --cpp \
    --no-union-value-namespacing \
    --gen-object-api \
    --keep-prefix \
    -I $(INPUT_DIR) \
    -o $(dir $@) \
    $(PRIVATE_INPUT_FBS) \
    || exit 33
[ -f $@ ] || exit 33
endef

intermediates := $(call local-generated-sources-dir)

# Generate utils/zlib/buffer_generated.h using FlatBuffer schema compiler.
UTILS_ZLIB_BUFFER_FBS := $(LOCAL_PATH)/utils/zlib/buffer.fbs
UTILS_ZLIB_BUFFER_H := $(intermediates)/utils/zlib/buffer_generated.h
$(UTILS_ZLIB_BUFFER_H): PRIVATE_INPUT_FBS := $(UTILS_ZLIB_BUFFER_FBS)
$(UTILS_ZLIB_BUFFER_H): INPUT_DIR := $(LOCAL_PATH)
$(UTILS_ZLIB_BUFFER_H): $(FLATC) $(UTILS_ZLIB_BUFFER_FBS)
	$(transform-fbs-to-cpp)
LOCAL_GENERATED_SOURCES += $(UTILS_ZLIB_BUFFER_H)

# Generate utils/intent/intent-config_generated.h using FlatBuffer schema compiler.
INTENT_CONFIG_FBS := $(LOCAL_PATH)/utils/intents/intent-config.fbs
INTENT_CONFIG_H := $(intermediates)/utils/intents/intent-config_generated.h
$(INTENT_CONFIG_H): PRIVATE_INPUT_FBS := $(INTENT_CONFIG_FBS)
$(INTENT_CONFIG_H): INPUT_DIR := $(LOCAL_PATH)
$(INTENT_CONFIG_H): $(FLATC) $(INTENT_CONFIG_FBS)
	$(transform-fbs-to-cpp)
LOCAL_GENERATED_SOURCES += $(INTENT_CONFIG_H)

# Generate annotator/model_generated.h using FlatBuffer schema compiler.
ANNOTATOR_MODEL_FBS := $(LOCAL_PATH)/annotator/model.fbs
ANNOTATOR_MODEL_H := $(intermediates)/annotator/model_generated.h
$(ANNOTATOR_MODEL_H): PRIVATE_INPUT_FBS := $(ANNOTATOR_MODEL_FBS)
$(ANNOTATOR_MODEL_H): INPUT_DIR := $(LOCAL_PATH)
$(ANNOTATOR_MODEL_H): $(FLATC) $(ANNOTATOR_MODEL_FBS) $(INTENT_CONFIG_H)
	$(transform-fbs-to-cpp)
LOCAL_GENERATED_SOURCES += $(ANNOTATOR_MODEL_H)
