package com.example.hellojni;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import java.lang.Thread;

public class FontActivity extends Activity
{
	private GLSurfaceView mGLSurfaceView;

	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		mGLSurfaceView = new GLSurfaceView(this);
		FontRenderer renderer = new FontRenderer();
		mGLSurfaceView.setRenderer(renderer);
		mGLSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
		setContentView(mGLSurfaceView);
		new Thread(new MyThread()).start();
	}
	
	class MyThread implements Runnable {
		private int times = 1000;
		public void run() {
			times = 1000;
			try {
				Thread.sleep(1000);
			} catch (Exception e) {
			}
			while (times > 0) {
				try {
					mGLSurfaceView.requestRender();
					Thread.sleep(30);
				} catch (Exception e) {
				}
				
				times--;
			}
		}
	}
}
