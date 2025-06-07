#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WHISPER_AVAILABLE
#include "transcription.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <audio_file.wav> [model_path]\n", argv[0]);
        printf("Example: %s test/test1.wav\n", argv[0]);
        printf("         %s test/test1.wav whisper.cpp/models/ggml-base.en.bin\n", argv[0]);
        return 1;
    }
    
    const char* audio_file = argv[1];
    const char* model_path = (argc > 2) ? argv[2] : "whisper.cpp/models/ggml-base.en.bin";
    
    printf("🧪 Testing Whisper transcription...\n");
    printf("Audio file: %s\n", audio_file);
    printf("Model: %s\n", model_path);
    printf("\n");
    
    // Initialize transcription
    if (transcription_init(model_path) != 0) {
        fprintf(stderr, "Failed to initialize transcription\n");
        return 1;
    }
    
    // Transcribe the audio file
    char result[1024];
    if (transcribe_file(audio_file, result, sizeof(result)) != 0) {
        fprintf(stderr, "Failed to transcribe audio file\n");
        transcription_cleanup();
        return 1;
    }
    
    // Output result
    printf("\n📝 Transcription result:\n");
    printf("----------------------------------------\n");
    printf("%s\n", result);
    printf("----------------------------------------\n");
    
    // Cleanup
    transcription_cleanup();
    
    printf("\n✅ Test complete!\n");
    return 0;
}

#else

int main(int argc, char* argv[]) {
    printf("❌ Whisper.cpp not available. Please run ./build.sh to build whisper.cpp first.\n");
    return 1;
}

#endif