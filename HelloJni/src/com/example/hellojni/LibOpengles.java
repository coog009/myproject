package com.example.hellojni;

import android.graphics.Bitmap;
import android.util.Log;

public class LibOpengles {

	static {
		Log.e("LibOpengles", "init");
		System.loadLibrary("gles");
		Log.e("LibOpengles", "finish");
	}
	
	public static native void init();
	public static native void create(int width, int height);
	public static native void draw();
	public static native void setbitmap(Bitmap bitmap, int width, int height);
}
