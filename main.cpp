/*
 * Realsense D435 color stream to HEVC with VAAPI encoding
 *
 * Copyright 2019 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Modified by Stephen Thomas-Dorin <stephen.thomasdorin@gmail.com>
 *
 */

/* This program is example how to use:
 * - VAAPI to hardware encode
 * - Realsense D435 color stream
 * - to HEVC raw video
 * - stored to disk as example
 *
 * See README.md for the details
 *
 */

// Hardware Video Encoder
#include "hve.h"

// Realsense API
#include <librealsense2/rs.hpp>

#include <fstream>
#include <iostream>
using namespace std;

//user supplied input
struct input_args
{
	int width;
	int height;
	int framerate;
	int seconds;
	char* filename;
};

bool main_loop(const input_args& input, rs2::pipeline& realsense, hve *avctx, ofstream& out_file);
void dump_frame_info(rs2::video_frame &frame);
void init_realsense(rs2::pipeline& pipe, const input_args& input);
int process_user_input(int argc, char* argv[], input_args* input, hve_config *config);

int main(int argc, char* argv[])
{
	struct hve *hardware_encoder;
	struct hve_config hardware_config = {0};
	struct input_args user_input = {0};

	if(process_user_input(argc, argv, &user_input, &hardware_config) < 0)
		return 1;

	ofstream out_file(user_input.filename, ofstream::binary);
	rs2::pipeline realsense;
	rs2::context ctx;

	if(!out_file)
		return 2;

	init_realsense(realsense, user_input);

	if( (hardware_encoder = hve_init(&hardware_config)) == NULL)
		return 3;
	
	bool status=main_loop(user_input, realsense, hardware_encoder, out_file);

	hve_close(hardware_encoder);

	out_file.close();

	if(status)
	{
		cout << "Finished successfully." << endl;
		cout << "Saved to: " << endl << endl << user_input.filename << endl;
	}

	return 0;
}

//true on success, false on failure
bool main_loop(const input_args& input, rs2::pipeline& realsense, hve *he, ofstream& out_file)
{
	const int frames = input.seconds * input.framerate;
	int f, failed;
	hve_frame frame = {0};
	uint8_t *color_data = NULL; //keep to use later for depth
	AVPacket *packet;

    // Capture 30 frames to give autoexposure, etc. a chance to settle
    for (auto i = 0; i < 10; ++i) realsense.wait_for_frames();

	for(f = 0; f < frames; ++f)
	{
		rs2::frameset frameset = realsense.wait_for_frames();
		rs2::video_frame c_frame = frameset.get_color_frame();

		// commented out for yuyv format (one plane) color stream
		// if(!color_data)
		// {   //prepare dummy color plane for NV12 format, half the size of Y
		// 	//we can't alloc it in advance, this is the first time we know realsense stride
		// 	int size = c_frame.get_stride_in_bytes()*c_frame.get_height()/2;
		// 	color_data = new uint8_t[size];
		// 	memset(color_data, 128, size);
		// }
		
		//supply realsense frame data as ffmpeg frame data
		frame.linesize[0] = c_frame.get_stride_in_bytes();
		// frame.linesize[1] = c_frame.get_stride_in_bytes();
		frame.data[0] = (uint8_t*) c_frame.get_data();
		// frame.data[1] = color_data;

		dump_frame_info(c_frame);

		if(hve_send_frame(he, &frame) != HVE_OK)
		{
			cerr << "failed to send frame to hardware" << endl;
			break;
		}
		
		while( (packet=hve_receive_packet(he, &failed)) )
		{ //do something with the data - here just dump to raw H.264 file
			cout << " encoded in: " << packet->size;
			out_file.write((const char*)packet->data, packet->size);
		}
		
		if(failed != HVE_OK)
		{
			cerr << "failed to encode frame" << endl;
			break;
		}	
	}
	
	//flush the encoder by sending NULL frame
	hve_send_frame(he, NULL);
	//drain the encoder from buffered frames
	while( (packet=hve_receive_packet(he, &failed)) )
	{ 
		cout << endl << "encoded in: " << packet->size;
		out_file.write((const char*)packet->data, packet->size);
	}
	cout << endl;
		
	// delete [] color_data; // save for depth data

	//all the requested frames processed?
	return f==frames;
}
void dump_frame_info(rs2::video_frame &f)
{
	cout << endl << f.get_frame_number ()
		<< ": width " << f.get_width() << " height " << f.get_height()
		<< " stride=" << f.get_stride_in_bytes() << " bytes "
		<< f.get_stride_in_bytes() * f.get_height();
}

void init_realsense(rs2::pipeline& pipe, const input_args& input)
{
	rs2::config cfg;
	cfg.enable_stream(RS2_STREAM_COLOR, 0, input.width, input.height, RS2_FORMAT_YUYV, input.framerate);

	rs2::pipeline_profile profile = pipe.start(cfg);
}

int process_user_input(int argc, char* argv[], input_args* input, hve_config *config)
{
	if(argc < 6)
	{
		cerr << "Usage: " << argv[0] << " <width> <height> <framerate> <seconds> <file>" << endl;
		cerr << endl << "examples: " << endl;
		cerr << argv[0] << " 640 360 30 5" << endl;
		cerr << argv[0] << " 640 360 30 5 output.hevc" << endl;

		return -1;
	}

	config->width = input->width = atoi(argv[1]);
	config->height = input->height = atoi(argv[2]);
	config->framerate = input->framerate = atoi(argv[3]);
	config->pixel_format = "yuyv422";
	
	input->seconds = atoi(argv[4]);
	
	input->filename = argv[5];
	
	return 0;
}
