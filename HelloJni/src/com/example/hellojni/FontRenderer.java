package com.example.hellojni;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.opengl.GLSurfaceView;
import android.util.Log;

public class FontRenderer implements GLSurfaceView.Renderer
{
	private Bitmap bitmap;
	private float count = 0;
	@Override
	public void onDrawFrame(GL10 gl) {
		// TODO Auto-generated method stub
//		Log.e("OnDrawFrame", "init");
		long time = System.currentTimeMillis();
		LibOpengles.draw();
		count += System.currentTimeMillis() - time;
		Log.e("OnDrawFrame", "speed time:" + count);
	}

	@Override
	public void onSurfaceChanged(GL10 gl, int width, int height) {
		Log.e("onSurfaceChanged", "init");
		LibOpengles.create(width, height);
		Log.e("onSurfaceChanged", "finish");
	}

	@Override
	public void onSurfaceCreated(GL10 gl, EGLConfig config) {
		Log.e("onSurfaceCreated", "init");
		LibOpengles.init();
		initFontBitmap();
		LibOpengles.setbitmap(bitmap, bitmap.getWidth(), bitmap.getHeight());
		Log.e("onSurfaceCreated", "finish");
	}
	
	public void initFontBitmap(){  
		String font = "需要渲染的文字测试！";  
		bitmap = Bitmap.createBitmap(1280, 1080, Bitmap.Config.ARGB_8888);  
		Canvas canvas = new Canvas(bitmap);  
		//背景颜色  
		canvas.drawColor(Color.LTGRAY);  
		Paint p = new Paint();  
		//字体设置  
		String fontType = "宋体";  
		Typeface typeface = Typeface.create(fontType, Typeface.BOLD);  
		//消除锯齿  
		p.setAntiAlias(true);  
		//字体为红色  
		p.setColor(Color.RED);  
		p.setTypeface(typeface);  
		p.setTextSize(28);  
		//绘制字体  
		canvas.drawText(font, 0, 100, p);  
	}
}
