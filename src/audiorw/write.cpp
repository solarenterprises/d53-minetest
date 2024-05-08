#include <vector>
#include <string>
#include <stdexcept>
#include <ciso646>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/audio_fifo.h>
#include <libavcodec/avcodec.h>
#include <oggz/oggz.h>
#include "fishsound/fishsound.h"
};

#include <sstream>
#include <fstream>

#include "audiorw.hpp"

using namespace audiorw;


long serialno = 0;
int g_b_o_s = 1;

static int encoded(FishSound* fsound, unsigned char* buf, long bytes, void* user_data)
{
	OGGZ* oggz = (OGGZ*)user_data;
	ogg_packet op;
	int err;

	op.packet = buf;
	op.bytes = bytes;
	op.b_o_s = g_b_o_s;
	op.e_o_s = 0;
	op.granulepos = fish_sound_get_frameno(fsound);
	op.packetno = -1;

	err = oggz_write_feed(oggz, &op, 0, 0, NULL);
	if (err < 0)
		switch (err)
		{
		case OGGZ_ERR_BAD_GUARD:
			printf("err: bad guard\n");
			break;
		case OGGZ_ERR_BOS:
			printf("err: BOS\n");
			break;
		case OGGZ_ERR_EOS:
			printf("err: EOS\n");
			break;
		case OGGZ_ERR_BAD_BYTES:
			printf("err: BAD BYTES\n");
			break;
		case OGGZ_ERR_BAD_B_O_S:
			printf("err: BAD BOS\n");
			break;
		case OGGZ_ERR_BAD_GRANULEPOS:
			printf("err: BAD GRANULEPOS\n");
			break;
		case OGGZ_ERR_BAD_PACKETNO:
			printf("err: BAD PACKETNO\n");
			break;
		case OGGZ_ERR_BAD_SERIALNO:
			printf("err: BAD SERIALNO\n");
			break;
		case OGGZ_ERR_BAD_OGGZ:
			printf("err: BAD OGGZ\n");
			break;
		case OGGZ_ERR_INVALID:
			printf("err: INVALID\n");
			break;
		case OGGZ_ERR_OUT_OF_MEMORY:
			printf("err: OUT OF MEMORY\n");
			break;
		default:
			printf("err: %d\n", err);
		}

	g_b_o_s = 0;

	return 0;
}

static size_t ogg_stream_write(void* user_handle, void* buf, size_t n) {
	OGGWriteStream* t = (OGGWriteStream*)user_handle;
	t->stream.write((char*)buf, n);
	return n;
}

OGGWriteStream::OGGWriteStream(Type _type, int sample_rate, int num_channels, bool _interleave) : type(_type), interleave(_interleave) {
	if ((oggz = oggz_new(OGGZ_WRITE | OGGZ_NONSTRICT)) == NULL) {
		printf("unable to open oggz\n");
		return;
	}

	oggz_io_set_write(oggz, ogg_stream_write, this);

	fsinfo.channels = num_channels;
	fsinfo.samplerate = sample_rate;
	fsinfo.format = FISH_SOUND_VORBIS;

	fsound = fish_sound_new(FISH_SOUND_ENCODE, &fsinfo);
	fish_sound_set_encoded_callback(fsound, encoded, oggz);

	if (interleave)
		fish_sound_set_interleave(fsound, 1);

	fish_sound_comment_add_byname(fsound, "Encoder", "fishsound-encode");
}

void OGGWriteStream::write(const char* data, std::streamsize dataSize) {
	num_bytes += dataSize;

	switch (type)
	{
	case audiorw::OGGWriteStream::SHORT:
		write_data((short*)data, dataSize / sizeof(short));
		break;
	case audiorw::OGGWriteStream::FLOAT:
		write_data((float*)data, dataSize / sizeof(float));
		break;
	default:
		throw "type not implemented!";
		break;
	}
}

#define ENCODE_BLOCK_SIZE (1024)

void OGGWriteStream::write_data(const short* data, std::streamsize dataSize) {
	write_samples.resize(write_samples.size() + dataSize);
	std::transform(
		data, data + dataSize,
		write_samples.end() - dataSize,
		[](const short& s) { return static_cast<float>(static_cast<double>(s) / std::numeric_limits<int16_t>::max()); });

	while (write_samples.size() >= ENCODE_BLOCK_SIZE) {
		writeToOGG();
	}
}

void OGGWriteStream::write_data(const float* data, std::streamsize dataSize) {
	write_samples.resize(write_samples.size() + dataSize);
	float* dest = write_samples.data() + write_samples.size() - dataSize;
	memcpy(dest, data, dataSize * sizeof(float));

	while (write_samples.size() >= ENCODE_BLOCK_SIZE) {
		writeToOGG();
	}
}

void OGGWriteStream::writeToOGG() {
	if (write_samples.empty())
		return;

	g_b_o_s = b_o_s;
	b_o_s = 0;

	memset(pcm, 0, sizeof(pcm));
	size_t len = sizeof(pcm) / sizeof(float);
	size_t cpy_size = std::min((size_t)ENCODE_BLOCK_SIZE, write_samples.size());
	memcpy(pcm, write_samples.data(), cpy_size * sizeof(float));
	write_samples.erase(write_samples.begin(), write_samples.begin() + cpy_size);

	if (interleave) {
		fish_sound_encode(fsound, (float**)pcm, ENCODE_BLOCK_SIZE);
	}
	else {
		float* pcm_channel[] = { pcm };
		fish_sound_encode(fsound, pcm_channel, ENCODE_BLOCK_SIZE);
	}
	oggz_run(oggz);
}

void OGGWriteStream::flush() {
	if (!fsound)
		return;

	closeOGG();
}

std::string OGGWriteStream::data() {
	OGGWriteStream::flush();
	return stream.str();
}

size_t OGGWriteStream::size() {
	return num_bytes;
}

void OGGWriteStream::closeOGG() {
	if (!fsound)
		return;

	while (!write_samples.empty())
		writeToOGG();

	fish_sound_flush(fsound);
	oggz_run(oggz);

	oggz_close(oggz);

	fish_sound_delete(fsound);
	fsound = nullptr;
}

OGGWriteFile::OGGWriteFile(Type type, const std::string& filename, int sample_rate, int num_channels) : OGGWriteStream(type, sample_rate, num_channels, false) {
	this->filename = filename;
}

void OGGWriteFile::flush() {
	OGGWriteStream::flush();

	if (didFlush)
		return;

	didFlush = true;
	std::string d = data();
	std::ofstream file(filename, std::ios::binary);
	file.write(d.c_str(), d.size());
	file.close();
}

int audiorw::write_ogg(
	const std::vector<std::vector<double>>& audio,
	double sample_rate,
	std::string& out_data) {

	if (audio.size() >= 2)
		throw "not implemented";

	std::vector<double> data = audio[0];

	std::vector<float> dataf(data.size());
	std::transform(data.begin(), data.end(), dataf.begin(), [](double sample) -> float { return static_cast<float>(sample); });

	OGGWriteStream stream(OGGWriteStream::FLOAT, sample_rate, audio.size(), true);
	stream.write((char*)dataf.data(), (std::streamsize)dataf.size() * sizeof(float));
	out_data = stream.data();
	return 1;
}

int audiorw::write_ogg(
	const std::vector<std::vector<short>>& audio,
	double sample_rate,
	std::string& out_data) {

	if (audio.size() >= 2)
		throw "not implemented";

	OGGWriteStream stream(OGGWriteStream::SHORT, sample_rate, audio.size(), true);
	stream.write((char*)audio[0].data(), (std::streamsize)audio[0].size() * sizeof(short));
	out_data = stream.data();
	return 1;
}

void audiorw::write(
    const std::vector<std::vector<double>> & audio,
    const std::string & filename,
    double sample_rate) {

  // Get a buffer for writing errors to
  size_t errbuf_size = 200;
  char errbuf[200];

  AVCodecContext * codec_context = NULL;
  AVFormatContext * format_context = NULL;
  SwrContext * resample_context = NULL;
  AVFrame * frame = NULL;
  AVPacket packet;
  memset(&packet, 0, sizeof(AVPacket));

  // Open the output file to write to it
  AVIOContext * output_io_context;
  int error = avio_open(
      &output_io_context, 
      filename.c_str(),
      AVIO_FLAG_WRITE);
  if (error < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    av_strerror(error, errbuf, errbuf_size);
    throw std::invalid_argument(
        "Could not open file:" + filename + "\n" + 
        "Error: " + std::string(errbuf));
  }

  // Create a format context for the output container format
  if (!(format_context = avformat_alloc_context())) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate output format context for file:" + filename);
  }

  // Associate the output context with the output file
  format_context -> pb = output_io_context;

  // Guess the desired output file type
  if (!(format_context->oformat = av_guess_format(NULL, filename.c_str(), NULL))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not find output file format for file: " + filename);
  }

  // Add the file pathname to the output context
  if (!(format_context -> url = av_strdup(filename.c_str()))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not process file path name for file: " + filename);
  }

  // Guess the encoder for the file
  /*AVCodecID codec_id = av_guess_codec(
      format_context -> oformat,
      NULL,
      filename.c_str(),
      NULL,
      AVMEDIA_TYPE_AUDIO);*/

  AVCodecID codec_id = AV_CODEC_ID_VORBIS;

  // Find an encoder based on the codec
  AVCodec * output_codec;
  if (!(output_codec = (AVCodec*)avcodec_find_encoder(codec_id))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not open codec with ID, " + std::to_string(codec_id) + ", for file: " + filename);
  }

  // Create a new audio stream in the output file container
  AVStream * stream;
  if (!(stream = avformat_new_stream(format_context, NULL))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not create new stream for output file: " + filename);
  }

  // Allocate an encoding context
  if (!(codec_context = avcodec_alloc_context3(output_codec))) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate an encoding context for output file: " + filename);
  }

  // Set the parameters of the stream
  codec_context -> channels = audio.size();
  codec_context -> channel_layout = av_get_default_channel_layout(audio.size());
  codec_context -> sample_rate = sample_rate;
  codec_context -> sample_fmt = output_codec -> sample_fmts[0];
  //codec_context -> bit_rate = OUTPUT_BIT_RATE;
  codec_context -> bit_rate = 64000;

  // Set the compliance level to experimental to use experimental codecs like Vorbis
  codec_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

  // Set the sample rate of the container
  stream -> time_base.den = sample_rate;
  stream -> time_base.num = 1;

  // Add a global header if necessary
  if (format_context -> oformat -> flags & AVFMT_GLOBALHEADER)
    codec_context -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  // Open the encoder for the audio stream to use
  if ((error = avcodec_open2(codec_context, output_codec, NULL)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    av_strerror(error, errbuf, errbuf_size);
    throw std::runtime_error(
        "Could not open output codec for file: " + filename + "\n" +
        "Error: " + std::string(errbuf));
  }

  // Make sure everything has been initialized correctly
  error = avcodec_parameters_from_context(stream->codecpar, codec_context);
  if (error < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not initialize stream parameters for file: " + filename);
  }

  // Initialize a resampler
  resample_context = swr_alloc_set_opts(
      NULL,
      // Output
      codec_context -> channel_layout,
      codec_context -> sample_fmt,
      sample_rate,
      // Input
      codec_context -> channel_layout,
      //AV_SAMPLE_FMT_DBL,
	  AV_SAMPLE_FMT_DBLP,
      sample_rate,
      0, NULL);
  if (!resample_context) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate resample context for file: " + filename);
  }

  // Open the context with the specified parameters
  if ((error = swr_init(resample_context)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not open resample context for file: " + filename);
  }

  // Write the header to the output file
  if ((error = avformat_write_header(format_context, NULL)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not write output file header for file: " + filename);
  }

  // Initialize the output frame
  if (!(frame = av_frame_alloc())) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not allocate output frame for file: " + filename);
  }
  // Allocate the frame size and format
  if (codec_context -> frame_size <= 0) {
    codec_context -> frame_size = DEFAULT_FRAME_SIZE;
  }
  frame -> nb_samples     = codec_context -> frame_size;
  frame -> channel_layout = codec_context -> channel_layout;
  frame -> format         = codec_context -> sample_fmt;
  frame -> sample_rate    = codec_context -> sample_rate;
  // Allocate the samples in the frame
  if ((error = av_frame_get_buffer(frame, 0)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    av_strerror(error, errbuf, errbuf_size);
    throw std::runtime_error(
        "Could not allocate output frame samples for file: " + filename + "\n" +
        "Error: " + std::string(errbuf));
  }

  // Construct a packet for the encoded frame
  av_init_packet(&packet);
  packet.data = NULL;
  packet.size = 0;

  // Write the samples to the audio
  int sample = 0;
  std::vector<double> audio_memory(audio.size() * codec_context->frame_size);
  double* audio_data = audio_memory.data();
  while (true) {
    if (sample < (int) audio[0].size()) {
      // Determine how much data to send
      int frame_size = std::min(codec_context -> frame_size, int(audio[0].size() - sample));
      frame -> nb_samples = frame_size;
      
      // Timestamp the frame
      frame -> pts = sample;

      // Choose a frame size of the audio
      for (int s = 0; s < frame_size; s++) {
        for (int channel = 0; channel < (int) audio.size(); channel++) {
          audio_data[audio.size() * s + channel] = audio[channel][sample+s];
        }
      }

      // Increment
      sample += frame_size;

      // Fill the frame with audio data
      const uint8_t * audio_data_ = reinterpret_cast<uint8_t *>(audio_data);
      if ((error = swr_convert(resample_context,
                               frame -> extended_data, frame_size,
                               &audio_data_          , frame_size)) < 0) {
        cleanup(codec_context, format_context, resample_context, frame, packet);
        av_strerror(error, errbuf, errbuf_size);
        throw std::runtime_error(
            "Could not resample frame for file: " + filename + "\n" +
            "Error: " + std::string(errbuf));
      }
    } else {
      // Enter draining mode
      av_frame_free(&frame);
      frame = NULL;
    }

    // Send a frame to the encoder to encode
    if ((error = avcodec_send_frame(codec_context, frame)) < 0) {
      cleanup(codec_context, format_context, resample_context, frame, packet);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not send packet for encoding for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

    // Receive the encoded frame from the encoder
    while ((error = avcodec_receive_packet(codec_context, &packet)) == 0) {
      // Write the encoded frame to the file
      if ((error = av_write_frame(format_context, &packet)) < 0) {
        cleanup(codec_context, format_context, resample_context, frame, packet);
        av_strerror(error, errbuf, errbuf_size);
        throw std::runtime_error(
            "Could not write frame for file: " + filename + "\n" +
            "Error: " + std::string(errbuf));
      }
    }

    // If we drain to the end, end the loop 
    if (error == AVERROR_EOF) {
      break;
    // If there was an error with the decoder
    } else if (error != AVERROR(EAGAIN)) {
      cleanup(codec_context, format_context, resample_context, frame, packet);
      av_strerror(error, errbuf, errbuf_size);
      throw std::runtime_error(
          "Could not encode frame for file: " + filename + "\n" +
          "Error: " + std::string(errbuf));
    }

  }

  // Write the trailer to the output file
  if ((error = av_write_trailer(format_context)) < 0) {
    cleanup(codec_context, format_context, resample_context, frame, packet);
    throw std::runtime_error(
        "Could not write output file trailer for file: " + filename);
  }

  // Cleanup
  cleanup(codec_context, format_context, resample_context, frame, packet);
}
