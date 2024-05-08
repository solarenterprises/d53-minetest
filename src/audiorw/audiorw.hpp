#pragma once

#include <vector>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <oggz/oggz.h>
#include "fishsound/fishsound.h"
};

#include "../util/stream.h"
#include <sstream>

namespace audiorw {

    static const int OUTPUT_BIT_RATE = 320000;
    static const int DEFAULT_FRAME_SIZE = 2048;

    std::vector<std::vector<double>> read(
        const std::string& filename,
        double& sample_rate,
        double start_seconds = 0,
        double end_seconds = -1);

    void write(
        const std::vector<std::vector<double>>& audio,
        const std::string& filename,
        double sample_rate);

    int write_ogg(
        const std::vector<std::vector<double>>& audio,
        double sample_rate,
        std::string& out_data);
    
    int write_ogg(
        const std::vector<std::vector<short>>& audio,
        double sample_rate,
        std::string& out_data);

    void cleanup(
        AVCodecContext* codec_context,
        AVFormatContext* format_context,
        SwrContext* resample_context,
        AVFrame* frame,
        AVPacket packet);

    class OGGWriteStream : public Stream {
    public:
        enum Type {
            SHORT = 0,
            FLOAT,
        };

    public:
        OGGWriteStream(Type type, int sample_rate, int num_channels, bool interleave);
        virtual void write(const char* data, std::streamsize dataSize);

        virtual void flush() override;
        std::string data();
        size_t size();

        std::ostringstream stream;

    private:
        void writeToOGG();
        void closeOGG();

        void write_data(const float* data, std::streamsize dataSize);
        void write_data(const short* data, std::streamsize dataSize);

        Type type;

        OGGZ* oggz = nullptr;
        FishSound* fsound = nullptr;
        FishSoundInfo fsinfo;

        int b_o_s = 1;
        float pcm[2048];

        std::vector<float> write_samples;
        size_t num_bytes = 0;

        bool interleave;
    };


    class OGGWriteFile : public OGGWriteStream {
    public:
        OGGWriteFile(Type type, const std::string& filename, int sample_rate, int num_channels);
        void flush();

    private:
        std::string filename;
        bool didFlush = false;
    };

}