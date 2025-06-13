#ifndef MODEL_DEFINITIONS_H
#define MODEL_DEFINITIONS_H

// Centralized model definitions for consistent cross-platform behavior

typedef struct {
    const char *name;
    const char *description;
    const char *size;
    const char *filename;  // Filename in ~/.yakety/models/
    const char *download_url;  // Empty string if not downloadable
} ModelInfo;

// Get available downloadable models
static const ModelInfo DOWNLOADABLE_MODELS[] = {
    {
        .name = "Whisper Tiny Q8",
        .description = "Ultra-lightweight model for basic transcription. Fastest processing with minimal memory usage, ideal for simple voice notes and testing. Works well on older hardware.",
        .size = "40 MB", 
        .filename = "ggml-tiny-q8_0.bin",
        .download_url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny-q8_0.bin"
    },
    {
        .name = "Whisper Large-v3 Turbo Q8",
        .description = "Premium model delivering the highest accuracy with optimized speed. Best for challenging audio conditions, technical content, and professional transcription. Requires modern hardware with sufficient memory.",
        .size = "800 MB", 
        .filename = "ggml-large-v3-turbo-q8_0.bin",
        .download_url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q8_0.bin"
    }
};

#define DOWNLOADABLE_MODELS_COUNT (sizeof(DOWNLOADABLE_MODELS) / sizeof(DOWNLOADABLE_MODELS[0]))

// Get bundled model info
static const ModelInfo BUNDLED_MODEL = {
    .name = "Whisper Base Q8",
    .description = "Bundled model optimized for real-time transcription. Fast and lightweight, ideal for general use and older hardware. Works well for clear audio in quiet environments.",
    .size = "75 MB",
    .filename = "",  // Empty means bundled
    .download_url = ""  // No download needed
};

// Get major supported languages
typedef struct {
    const char *name;
    const char *code;
} LanguageInfo;

static const LanguageInfo SUPPORTED_LANGUAGES[] = {
    {"English", "en"},
    {"Spanish", "es"}, 
    {"French", "fr"},
    {"German", "de"},
    {"Italian", "it"},
    {"Portuguese", "pt"},
    {"Chinese", "zh"},
    {"Japanese", "ja"},
    {"Korean", "ko"},
    {"Russian", "ru"},
    {"Arabic", "ar"},
    {"Hindi", "hi"},
    {"Dutch", "nl"},
    {"Swedish", "sv"},
    {"Polish", "pl"}
};

#define SUPPORTED_LANGUAGES_COUNT (sizeof(SUPPORTED_LANGUAGES) / sizeof(SUPPORTED_LANGUAGES[0]))

// Helper function to find model info by filename
static const ModelInfo* find_model_by_filename(const char* filename) {
    if (!filename) return NULL;
    
    // Check downloadable models
    for (int i = 0; i < DOWNLOADABLE_MODELS_COUNT; i++) {
        if (strcmp(DOWNLOADABLE_MODELS[i].filename, filename) == 0) {
            return &DOWNLOADABLE_MODELS[i];
        }
    }
    
    // Check bundled model
    if (strlen(BUNDLED_MODEL.filename) == 0) {
        // Bundled model matches common names
        if (strcmp(filename, "ggml-base-q8_0.bin") == 0) {
            return &BUNDLED_MODEL;
        }
    } else if (strcmp(BUNDLED_MODEL.filename, filename) == 0) {
        return &BUNDLED_MODEL;
    }
    
    return NULL;
}

#endif // MODEL_DEFINITIONS_H