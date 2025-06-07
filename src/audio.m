#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Audio configurations
const AudioConfig WHISPER_AUDIO_CONFIG = {
    .sample_rate = 16000,
    .channels = 1,
    .bits_per_sample = 16
};

const AudioConfig HIGH_QUALITY_AUDIO_CONFIG = {
    .sample_rate = 44100,
    .channels = 2,
    .bits_per_sample = 16
};

// Internal AudioRecorder structure
struct AudioRecorder {
    AudioConfig config;
    AVAudioEngine* engine;
    AVAudioInputNode* inputNode;
    AVAudioFile* audioFile;
    NSURL* fileURL;
    
    // For buffer recording
    NSMutableData* audioBuffer;
    float* audioData;
    size_t audioDataSize;
    
    bool isRecording;
    NSDate* startTime;
};

AudioRecorder* audio_recorder_create(const AudioConfig* config) {
    AudioRecorder* recorder = calloc(1, sizeof(AudioRecorder));
    if (!recorder) return NULL;
    
    recorder->config = *config;
    recorder->engine = [[AVAudioEngine alloc] init];
    recorder->inputNode = [recorder->engine inputNode];
    recorder->audioBuffer = [[NSMutableData alloc] init];
    
    return recorder;
}

int audio_recorder_start_file(AudioRecorder* recorder, const char* filename) {
    if (!recorder || recorder->isRecording) return -1;
    
    @try {
        // Create file URL
        NSString* nsFilename = [NSString stringWithUTF8String:filename];
        recorder->fileURL = [NSURL fileURLWithPath:nsFilename];
        
        // Get input format
        AVAudioFormat* inputFormat = [recorder->inputNode outputFormatForBus:0];
        
        // Create our desired output format
        AVAudioFormat* outputFormat = [[AVAudioFormat alloc] 
            initStandardFormatWithSampleRate:recorder->config.sample_rate
            channels:recorder->config.channels];
        
        if (!outputFormat) {
            NSLog(@"Failed to create output format");
            return -1;
        }
        
        // Create audio file with our desired format
        NSError* error = nil;
        recorder->audioFile = [[AVAudioFile alloc] 
            initForWriting:recorder->fileURL
            settings:outputFormat.settings
            error:&error];
        
        if (error) {
            NSLog(@"Failed to create audio file: %@", error.localizedDescription);
            return -1;
        }
        
        // Create converter for format conversion
        AVAudioConverter* converter = [[AVAudioConverter alloc] 
            initFromFormat:inputFormat 
            toFormat:outputFormat];
        
        if (!converter) {
            NSLog(@"Failed to create audio converter");
            return -1;
        }
        
        // Install tap with conversion
        [recorder->inputNode installTapOnBus:0 
            bufferSize:1024 
            format:inputFormat
            block:^(AVAudioPCMBuffer* buffer, AVAudioTime* when) {
                (void)when; // silence warning
                
                // Calculate output buffer capacity
                AVAudioFrameCount outputCapacity = (AVAudioFrameCount)(buffer.frameLength * outputFormat.sampleRate / inputFormat.sampleRate) + 1;
                AVAudioPCMBuffer* outputBuffer = [[AVAudioPCMBuffer alloc] 
                    initWithPCMFormat:outputFormat 
                    frameCapacity:outputCapacity];
                
                if (!outputBuffer) {
                    NSLog(@"Failed to create output buffer");
                    return;
                }
                
                // Convert audio
                __block BOOL inputConsumed = NO;
                NSError* conversionError = nil;
                
                AVAudioConverterOutputStatus status = [converter convertToBuffer:outputBuffer 
                    error:&conversionError 
                    withInputFromBlock:^AVAudioBuffer*(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus* outStatus) {
                        (void)inNumberOfPackets;
                        if (inputConsumed) {
                            *outStatus = AVAudioConverterInputStatus_NoDataNow;
                            return nil;
                        } else {
                            *outStatus = AVAudioConverterInputStatus_HaveData;
                            inputConsumed = YES;
                            return buffer;
                        }
                    }];
                
                if (status == AVAudioConverterOutputStatus_HaveData && !conversionError) {
                    NSError* writeError = nil;
                    [recorder->audioFile writeFromBuffer:outputBuffer error:&writeError];
                    if (writeError) {
                        NSLog(@"Error writing audio: %@", writeError.localizedDescription);
                    }
                } else if (conversionError) {
                    NSLog(@"Audio conversion error: %@", conversionError.localizedDescription);
                }
            }];
        
        // Start the engine
        [recorder->engine startAndReturnError:&error];
        if (error) {
            NSLog(@"Failed to start audio engine: %@", error.localizedDescription);
            return -1;
        }
        
        recorder->isRecording = true;
        recorder->startTime = [NSDate now];
        
        return 0;
    }
    @catch (NSException* exception) {
        NSLog(@"Exception starting recording: %@", exception.reason);
        return -1;
    }
}

int audio_recorder_start_buffer(AudioRecorder* recorder) {
    if (!recorder || recorder->isRecording) return -1;
    
    @try {
        // Clear previous buffer data
        [recorder->audioBuffer setLength:0];
        if (recorder->audioData) {
            free(recorder->audioData);
            recorder->audioData = NULL;
            recorder->audioDataSize = 0;
        }
        
        // Use the input node's native format for recording
        AVAudioFormat* inputFormat = [recorder->inputNode outputFormatForBus:0];
        
        NSLog(@"Hardware format: %.0fHz, %u channels", inputFormat.sampleRate, inputFormat.channelCount);
        
        // Install tap on input node to capture to buffer
        [recorder->inputNode installTapOnBus:0 
            bufferSize:1024 
            format:inputFormat 
            block:^(AVAudioPCMBuffer* buffer, AVAudioTime* when) {
                (void)when; // silence warning
                
                // Store raw audio data - we'll convert later
                if (buffer.format.commonFormat == AVAudioPCMFormatFloat32) {
                    // Handle float format
                    float* channelData = buffer.floatChannelData[0];
                    NSUInteger frameCount = buffer.frameLength;
                    NSUInteger channels = buffer.format.channelCount;
                    
                    // For multi-channel, we need to handle interleaved vs non-interleaved
                    if (buffer.format.isInterleaved) {
                        // Interleaved format
                        [recorder->audioBuffer appendBytes:channelData 
                            length:frameCount * channels * sizeof(float)];
                    } else {
                        // Non-interleaved format - just take first channel for now
                        [recorder->audioBuffer appendBytes:channelData 
                            length:frameCount * sizeof(float)];
                    }
                } else {
                    NSLog(@"Unsupported audio format in tap: %ld", (long)buffer.format.commonFormat);
                }
            }];
        
        // Start the engine
        NSError* error = nil;
        [recorder->engine startAndReturnError:&error];
        if (error) {
            NSLog(@"Failed to start audio engine: %@", error.localizedDescription);
            return -1;
        }
        
        recorder->isRecording = true;
        recorder->startTime = [NSDate now];
        
        return 0;
    }
    @catch (NSException* exception) {
        NSLog(@"Exception starting buffer recording: %@", exception.reason);
        return -1;
    }
}

int audio_recorder_stop(AudioRecorder* recorder) {
    if (!recorder || !recorder->isRecording) return -1;
    
    @try {
        // Stop the engine
        [recorder->engine stop];
        
        // Remove tap
        [recorder->inputNode removeTapOnBus:0];
        
        // Close audio file if recording to file
        if (recorder->audioFile) {
            recorder->audioFile = nil;
        }
        
        // Convert buffer data using AVAudioConverter
        if (recorder->audioBuffer.length > 0) {
            AVAudioFormat* hwFormat = [recorder->inputNode outputFormatForBus:0];
            AVAudioFormat* targetFormat = [[AVAudioFormat alloc] 
                initStandardFormatWithSampleRate:recorder->config.sample_rate
                channels:recorder->config.channels];
            
            // Create input buffer from raw data
            size_t frameCount = recorder->audioBuffer.length / sizeof(float) / hwFormat.channelCount;
            NSLog(@"Buffer: %zu bytes, %zu frames", recorder->audioBuffer.length, frameCount);
            
            if (frameCount == 0) {
                NSLog(@"No frames to convert");
                recorder->audioDataSize = 0;
                recorder->audioData = NULL;
                return 0;
            }
            
            AVAudioPCMBuffer* inputBuffer = [[AVAudioPCMBuffer alloc] 
                initWithPCMFormat:hwFormat 
                frameCapacity:(AVAudioFrameCount)frameCount];
            inputBuffer.frameLength = (AVAudioFrameCount)frameCount;
            
            if (!inputBuffer || !inputBuffer.floatChannelData[0]) {
                NSLog(@"Failed to create input buffer");
                recorder->audioDataSize = 0;
                recorder->audioData = NULL;
                return 0;
            }
            
            // Copy data to input buffer
            if (hwFormat.channelCount == 1) {
                // Mono - direct copy
                memcpy(inputBuffer.floatChannelData[0], recorder->audioBuffer.bytes, recorder->audioBuffer.length);
            } else {
                // Multi-channel - need to handle differently
                NSLog(@"Multi-channel audio not handled yet");
                recorder->audioDataSize = 0;
                recorder->audioData = NULL;
                return 0;
            }
            
            // Create converter
            AVAudioConverter* converter = [[AVAudioConverter alloc] 
                initFromFormat:hwFormat 
                toFormat:targetFormat];
            
            if (!converter) {
                NSLog(@"Failed to create converter from %.0fHz to %dHz", hwFormat.sampleRate, recorder->config.sample_rate);
                recorder->audioDataSize = 0;
                recorder->audioData = NULL;
                return 0;
            }
            
            NSLog(@"Created converter: %.0fHz %uch -> %dHz %dch", 
                  hwFormat.sampleRate, hwFormat.channelCount,
                  recorder->config.sample_rate, recorder->config.channels);
            
            if (converter) {
                // Calculate output buffer size
                AVAudioFrameCount outputFrames = (AVAudioFrameCount)(frameCount * targetFormat.sampleRate / hwFormat.sampleRate) + 1;
                AVAudioPCMBuffer* outputBuffer = [[AVAudioPCMBuffer alloc] 
                    initWithPCMFormat:targetFormat 
                    frameCapacity:outputFrames];
                
                // Convert audio
                __block BOOL inputConsumed = NO;
                NSError* error = nil;
                
                AVAudioConverterOutputStatus status = [converter convertToBuffer:outputBuffer 
                    error:&error 
                    withInputFromBlock:^AVAudioBuffer*(AVAudioPacketCount inNumberOfPackets, AVAudioConverterInputStatus* outStatus) {
                        (void)inNumberOfPackets;
                        if (inputConsumed) {
                            *outStatus = AVAudioConverterInputStatus_NoDataNow;
                            return nil;
                        } else {
                            *outStatus = AVAudioConverterInputStatus_HaveData;
                            inputConsumed = YES;
                            return inputBuffer;
                        }
                    }];
                
                NSLog(@"Conversion status: %ld, output frames: %u", (long)status, outputBuffer.frameLength);
                
                // Status 1 is AVAudioConverterOutputStatus_InputRanDry - converter processed all input
                // This is a successful conversion when we have output frames
                if (outputBuffer.frameLength > 0 && status != AVAudioConverterOutputStatus_Error) {
                    // Copy converted data
                    recorder->audioDataSize = outputBuffer.frameLength;
                    recorder->audioData = malloc(outputBuffer.frameLength * sizeof(float));
                    if (recorder->audioData) {
                        memcpy(recorder->audioData, outputBuffer.floatChannelData[0], 
                               outputBuffer.frameLength * sizeof(float));
                    }
                    NSLog(@"Converted %.0fHz %uch to %dHz %dch (%zu samples)", 
                          hwFormat.sampleRate, hwFormat.channelCount,
                          recorder->config.sample_rate, recorder->config.channels,
                          recorder->audioDataSize);
                } else {
                    NSLog(@"Audio conversion failed with status %ld, output frames: %u, error: %@", 
                          (long)status, outputBuffer.frameLength, error ? error.localizedDescription : @"None");
                    recorder->audioDataSize = 0;
                    recorder->audioData = NULL;
                }
            } else {
                NSLog(@"Failed to create audio converter");
                recorder->audioDataSize = 0;
                recorder->audioData = NULL;
            }
        }
        
        recorder->isRecording = false;
        
        return 0;
    }
    @catch (NSException* exception) {
        NSLog(@"Exception stopping recording: %@", exception.reason);
        return -1;
    }
}

const float* audio_recorder_get_data(AudioRecorder* recorder, size_t* out_size) {
    if (!recorder || !out_size) return NULL;
    
    *out_size = recorder->audioDataSize;
    return recorder->audioData;
}

double audio_recorder_get_duration(AudioRecorder* recorder) {
    if (!recorder || !recorder->startTime) return 0.0;
    
    if (recorder->isRecording) {
        return [[NSDate now] timeIntervalSinceDate:recorder->startTime];
    } else {
        // Calculate duration from audio data
        if (recorder->audioDataSize > 0) {
            return (double)recorder->audioDataSize / recorder->config.sample_rate;
        }
    }
    
    return 0.0;
}

bool audio_recorder_is_recording(AudioRecorder* recorder) {
    return recorder ? recorder->isRecording : false;
}

void audio_recorder_destroy(AudioRecorder* recorder) {
    if (!recorder) return;
    
    if (recorder->isRecording) {
        audio_recorder_stop(recorder);
    }
    
    if (recorder->audioData) {
        free(recorder->audioData);
    }
    
    recorder->engine = nil;
    recorder->inputNode = nil;
    recorder->audioFile = nil;
    recorder->fileURL = nil;
    recorder->audioBuffer = nil;
    recorder->startTime = nil;
    
    free(recorder);
}

int audio_request_permissions(void) {
    @try {
        __block int result = -1;
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio 
            completionHandler:^(BOOL granted) {
                result = granted ? 0 : -1;
                dispatch_semaphore_signal(semaphore);
            }];
        
        // Wait for permission response (timeout after 5 seconds)
        dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC);
        if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
            NSLog(@"Timeout waiting for audio permission");
            return -1;
        }
        
        return result;
    }
    @catch (NSException* exception) {
        NSLog(@"Exception requesting audio permissions: %@", exception.reason);
        return -1;
    }
}