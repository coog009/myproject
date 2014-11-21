package io.vec.demo.mediacodec;

import java.io.IOException;
import java.nio.ByteBuffer;

import android.app.Activity;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class DecodeActivity extends Activity implements SurfaceHolder.Callback {
	private static final String SAMPLE = "/sdcard/wangfei.avi";
	private PlayerThread mPlayer = null;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		SurfaceView sv = new SurfaceView(this);
		sv.getHolder().addCallback(this);
		setContentView(sv);
	}

	protected void onDestroy() {
		super.onDestroy();
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
		if (mPlayer == null) {
			mPlayer = new PlayerThread(holder.getSurface());
			mPlayer.start();
		}
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		if (mPlayer != null) {
			mPlayer.interrupt();
		}
	}

	private class PlayerThread extends Thread {
		private MediaExtractor extractor;
		private MediaCodec decoder;
		private Surface surface;

		private MediaCodec audio_codec;
		
		private int video_track;
		private int audio_track;
		
		private AudioTrack audioTrack;
		
		public PlayerThread(Surface surface) {
			this.surface = surface;
		}

		@Override
		public void run() {
			extractor = new MediaExtractor();
			try {
				extractor.setDataSource(SAMPLE);
			} catch (IOException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}

			for (int i = 0; i < extractor.getTrackCount(); i++) {
				MediaFormat format = extractor.getTrackFormat(i);
				String mime = format.getString(MediaFormat.KEY_MIME);
				Log.e("mediacodecplayer", format.toString());
				if (mime.startsWith("video/")) {
					extractor.selectTrack(i);
					decoder = MediaCodec.createDecoderByType(mime);
					decoder.configure(format, surface, null, 0);
					video_track = i;
//					break;
				} else if (mime.startsWith("audio/")) {
					extractor.selectTrack(i);
					audio_codec = MediaCodec.createDecoderByType(mime);
					audio_codec.configure(format, null, null, 0);
					audio_track = i;
					
					audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, format.getInteger(MediaFormat.KEY_SAMPLE_RATE),
							format.getInteger(MediaFormat.KEY_CHANNEL_COUNT), AudioFormat.ENCODING_PCM_16BIT, 8192 * 2,
							AudioTrack.MODE_STREAM);
					audioTrack.setPlaybackRate(format.getInteger(MediaFormat.KEY_SAMPLE_RATE) * format.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
				}
			}

			if (decoder == null) {
				Log.e("DecodeActivity", "Can't find video info!");
				return;
			}

			decoder.start();
			audio_codec.start();
			audioTrack.play();

			ByteBuffer[] inputBuffers = decoder.getInputBuffers();
			ByteBuffer[] outputBuffers = decoder.getOutputBuffers();
			
			ByteBuffer[] audio_inputBuffers = audio_codec.getInputBuffers();
			ByteBuffer[] audio_outputBuffers = audio_codec.getOutputBuffers();
			
			BufferInfo info = new BufferInfo();
			boolean isEOS = false;
			long startMs = System.currentTimeMillis();

			while (!Thread.interrupted()) {
				if (!isEOS) {
					int track_index = extractor.getSampleTrackIndex();

					if (track_index == video_track) {
						int inIndex = decoder.dequeueInputBuffer(10000);
						if (inIndex >= 0) {
							ByteBuffer buffer = inputBuffers[inIndex];
							int sampleSize = extractor.readSampleData(buffer, 0);
							if (sampleSize < 0) {
								// We shouldn't stop the playback at this point, just pass the EOS
								// flag to decoder, we will get it again from the
								// dequeueOutputBuffer
								Log.d("DecodeActivity", "InputBuffer BUFFER_FLAG_END_OF_STREAM");
								decoder.queueInputBuffer(inIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
								isEOS = true;
							} else {
								decoder.queueInputBuffer(inIndex, 0, sampleSize, extractor.getSampleTime(), 0);
								extractor.advance();
							}
						}

						int outIndex = decoder.dequeueOutputBuffer(info, 10000);
						switch (outIndex) {
						case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
							Log.d("DecodeActivity", "INFO_OUTPUT_BUFFERS_CHANGED");
							outputBuffers = decoder.getOutputBuffers();
							break;
						case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
							Log.d("DecodeActivity", "New format " + decoder.getOutputFormat());
							break;
						case MediaCodec.INFO_TRY_AGAIN_LATER:
							Log.d("DecodeActivity", "dequeueOutputBuffer timed out!");
							break;
						default:
							ByteBuffer buffer = outputBuffers[outIndex];
							Log.v("DecodeActivity", "We can't use this buffer but render it due to the API limit, " + buffer);

							// We use a very simple clock to keep the video FPS, or the video
							// playback will be too fast
							while (info.presentationTimeUs / 1000 > System.currentTimeMillis() - startMs) {
								try {
									sleep(10);
								} catch (InterruptedException e) {
									e.printStackTrace();
									break;
								}
							}
							decoder.releaseOutputBuffer(outIndex, true);
							break;
						}
					} else if (track_index == audio_track) {
						int inIndex = audio_codec.dequeueInputBuffer(10000);
						if (inIndex >= 0) {
							ByteBuffer buffer = audio_inputBuffers[inIndex];
							int sampleSize = extractor.readSampleData(buffer, 0);
							if (sampleSize < 0) {
								// We shouldn't stop the playback at this point, just pass the EOS
								// flag to decoder, we will get it again from the
								// dequeueOutputBuffer
								Log.d("DecodeActivity", "InputBuffer BUFFER_FLAG_END_OF_STREAM");
								audio_codec.queueInputBuffer(inIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
								isEOS = true;
							} else {
								audio_codec.queueInputBuffer(inIndex, 0, sampleSize, extractor.getSampleTime(), 0);
								extractor.advance();
							}
						}

						int outIndex = audio_codec.dequeueOutputBuffer(info, 10000);
						switch (outIndex) {
						case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
							Log.d("DecodeActivity", "INFO_OUTPUT_BUFFERS_CHANGED");
							outputBuffers = audio_codec.getOutputBuffers();
							break;
						case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
							Log.d("DecodeActivity", "New format " + audio_codec.getOutputFormat());
							break;
						case MediaCodec.INFO_TRY_AGAIN_LATER:
							Log.d("DecodeActivity", "dequeueOutputBuffer timed out!");
							break;
						default:
							ByteBuffer buffer = audio_outputBuffers[outIndex];
							Log.v("DecodeActivity", "We can't use this buffer but render it due to the API limit, " + buffer);

							// We use a very simple clock to keep the video FPS, or the video
							// playback will be too fast
							byte[] chunk = new byte[info.size];
							buffer.get(chunk); // Read the buffer all at once
							buffer.clear(); // ** MUST DO!!! OTHERWISE THE NEXT TIME YOU GET THIS SAME BUFFER BAD THINGS WILL HAPPEN

							if (chunk.length > 0) {
								audioTrack.write(chunk, 0, chunk.length);
							}
							
							audio_codec.releaseOutputBuffer(outIndex, false);
							break;
						}
					}
				}

				// All decoded frames have been rendered, we can stop playing now
				if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
					Log.d("DecodeActivity", "OutputBuffer BUFFER_FLAG_END_OF_STREAM");
					break;
				}
			}

			decoder.stop();
			decoder.release();
			extractor.release();
		}
	}
}