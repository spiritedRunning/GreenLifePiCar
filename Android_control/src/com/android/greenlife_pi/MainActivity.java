package com.android.greenlife_pi;

import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStreamWriter;
import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.BitmapFactory;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.StrictMode;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import com.example.greenlife_pi.R;

public class MainActivity extends Activity implements OnClickListener
{
	protected static String tag = "MainActivity";
	private ImageView picView;
	private ImageButton btnForward;
	private ImageButton btnBack;
	private ImageButton btnLeft;
	private ImageButton btnRight;
	private ImageButton btnStop;
	private TextView tvSearchwifi;
	
	// for Gravity interactiion
	private SensorEventListener GrivatySeListener;
	private SensorManager mSensorManager;
	private Sensor sensor;
	public float x, y, z;
	
	CheckBox cbGrivaty = null;
	CheckBox caputrePic = null;
	boolean bGravity = false;
	
	// private String dev_ip = "192.168.43.118"; // wifi address
	private String dev_ip = "192.168.1.109";
	private int cmd_port = 5888;
	private int img_port = 12088;
	private Socket pic_socket = null;
	
	private String forward_cmd = "0x55";
	private String back_cmd = "0x56";
	private String left_cmd = "0x57";
	private String right_cmd = "0x58";
	private String stop_cmd = "0x59";
	
	private static final int GO_FORWARD = 100;
	private static final int GO_BACK = 101;
	private static final int GO_LEFT = 102;
	private static final int GO_RIGHT = 103;
	private static final int GO_STOP = 104;
	
	private Thread picThread = null;
	private static final int MSG_IMGSHOW = 0x1337;
	private Bitmap bitmap;
	
	int curAction = GO_STOP;
	int nextAction;
	
	private static int interrupttime = 0;
	
	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		
		popAddrdlg();
		init();
	}
	
	private void popAddrdlg()
	{
		final EditText input = new EditText(this);
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle("输入被控端IP").setIcon(android.R.drawable.ic_dialog_info).setView(input)
				.setNegativeButton("取消", null);
		builder.setPositiveButton("确定", new DialogInterface.OnClickListener() {
			
			public void onClick(DialogInterface dialog, int which)
			{
				dev_ip = input.getText().toString();
				Log.v(tag, "remote ip: " + dev_ip);
			}
		});
		builder.show();
		
	}
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		// Inflate the menu; this adds items to the action bar if it is present.
		// getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}
	
	private void init()
	{
		picView = (ImageView) findViewById(R.id.image_capture);
		btnForward = (ImageButton) findViewById(R.id.button_forward);
		btnBack = (ImageButton) findViewById(R.id.button_back);
		btnLeft = (ImageButton) findViewById(R.id.button_left);
		btnRight = (ImageButton) findViewById(R.id.button_right);
		btnStop = (ImageButton) findViewById(R.id.button_stop);
		tvSearchwifi = (TextView) findViewById(R.id.text_wifi_connect);
		
		// register click event
		btnForward.setOnClickListener(this);
		btnBack.setOnClickListener(this);
		btnLeft.setOnClickListener(this);
		btnRight.setOnClickListener(this);
		btnStop.setOnClickListener(this);
		tvSearchwifi.setOnClickListener(this);
		
		mSensorManager = (SensorManager) getSystemService(SENSOR_SERVICE); // 初始化感应器
		sensor = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER); // 实例化一个重力感应sensor
		
		cbGrivaty = (CheckBox) findViewById(R.id.startGavity);
		caputrePic = (CheckBox) findViewById(R.id.videoCaputre);
		
		// initGravitySensor();
		registerGravityEvent();
		
		initCheckListener();
		
		// android 4.4.2 networking operation can't in main thread
		StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder().detectDiskReads().detectDiskWrites()
				.detectNetwork().penaltyLog().build());
		
	}
	
	private void initCheckListener()
	{
		cbGrivaty.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
			@Override
			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked)
			{
				if (isChecked)
				{
					// initGravitySensor();
					mSensorManager.registerListener(GrivatySeListener, sensor, SensorManager.SENSOR_DELAY_GAME);
					// registerGravityEvent();
					Log.v(tag, "start gravity listener");
				}
				else
				{
					// 无法停止，还未找到原因
					mSensorManager.unregisterListener(GrivatySeListener);
					Log.v(tag, "unstart gravity listener");
				}
			}
		});
		
		caputrePic.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
			@Override
			public void onCheckedChanged(CompoundButton buttonView, boolean isChecked)
			{
				if (isChecked)
				{
					Log.v(tag, "thrans thread start");
					picThread = new Thread(new TransThread());
					picThread.start();
				}
				else
				{
					if (picThread != null && picThread.isAlive())
					{
						picThread.interrupt();
						Log.e(tag, "picThread status: " + picThread.isInterrupted());
					}
				}
			}
		});
	}
	
	Handler bmpHandler = new Handler() {
		public void handleMessage(Message msg)
		{
			switch (msg.what)
			{
				case MSG_IMGSHOW:
				{
					picView.setImageBitmap(bitmap);
					break;
				}
				default:
					break;
			}
			super.handleMessage(msg);
		}
	};
	
	void registerGravityEvent()
	{
		
		Log.v(tag, "registerGravityEvent");
		// 设置重力感应的监听事件
		GrivatySeListener = new SensorEventListener() {
			
			public void onSensorChanged(SensorEvent event)
			{
				
				// 获取X/Y/Z轴的重力加速度值
				int x = (int) event.values[0];
				int y = (int) event.values[1];
				int z = (int) event.values[2];
				
				// 前
				if ((x >= 0 && x <= 1) && (y >= -5 && y <= -2))
				{
					Log.v(tag, "[forword]x = " + x + ", y = " + y + ",z= " + z);
					
					nextAction = GO_FORWARD;
				}
				
				// 后
				else if ((x >= 0 && x <= 1) && (y >= 3 && y <= 6))
				{
					Log.v(tag, "[backward]x = " + x + ", y = " + y + ",z= " + z);
					
					nextAction = GO_BACK;
				}
				// 左
				else if ((x >= 4 && x <= 8) && (y >= 0 && y <= 1))
				{
					Log.v(tag, "[left]x = " + x + ", y = " + y + ",z= " + z);
					nextAction = GO_LEFT;
				}
				
				// 右
				else if ((x >= -8 && x <= -4) && (y >= 0 && y <= 1))
				{
					Log.v(tag, "[right]x = " + x + ", y = " + y + ",z= " + z);
					nextAction = GO_RIGHT;
				}
				
				else if (x == 0 && (y >= 0 && y <= 2))
				{
					Log.v(tag, "[stop]x = " + x + ", y = " + y + ",z= " + z);
					nextAction = GO_STOP;
				}
				else
				{
					Log.v(tag, "invalid command: x = " + x + ", y = " + y + ",z= " + z);
				}
				
				if (curAction != nextAction)
				{
					curAction = nextAction;
					
					if (curAction == GO_FORWARD)
					{
						commandSendForward();
					}
					else if (curAction == GO_BACK)
					{
						commandSendBackward();
					}
					else if (curAction == GO_LEFT)
					{
						commandSendLeft();
					}
					else if (curAction == GO_RIGHT)
					{
						commandSendRight();
					}
					else if (curAction == GO_STOP)
					{
						commandSendStop();
					}
				}
			}
			
			@Override
			public void onAccuracyChanged(Sensor sensor, int accuracy)
			{
				// TODO Auto-generated method stub
				
			}
		};
	}
	
	private void commandSendForward()
	{
		try
		{
			SendMsg(dev_ip, cmd_port, forward_cmd);
			Log.v(tag, "send forward_cmd to board");
		}
		catch (UnknownHostException e)
		{
			e.printStackTrace();
		}
		catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	private void commandSendBackward()
	{
		try
		{
			SendMsg(dev_ip, cmd_port, back_cmd);
			Log.v(tag, "send backward_cmd to board");
		}
		catch (UnknownHostException e)
		{
			e.printStackTrace();
		}
		catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	private void commandSendLeft()
	{
		try
		{
			SendMsg(dev_ip, cmd_port, left_cmd);
			Log.v(tag, "send left_cmd to board");
		}
		catch (UnknownHostException e)
		{
			e.printStackTrace();
		}
		catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	private void commandSendRight()
	{
		try
		{
			SendMsg(dev_ip, cmd_port, right_cmd);
			Log.v(tag, "send right_cmd to board");
		}
		catch (UnknownHostException e)
		{
			e.printStackTrace();
		}
		catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	private void commandSendStop()
	{
		try
		{
			SendMsg(dev_ip, cmd_port, stop_cmd);
			Log.v(tag, "send stop_cmd to board");
		}
		catch (UnknownHostException e)
		{
			e.printStackTrace();
		}
		catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	@Override
	public void onClick(View v)
	{
		switch (v.getId())
		{
			case R.id.button_forward:
				commandSendForward();
				break;
			case R.id.button_back:
				commandSendBackward();
				break;
			case R.id.button_left:
				commandSendLeft();
				break;
			case R.id.button_right:
				commandSendRight();
				break;
			case R.id.button_stop:
				commandSendStop();
				break;
			case R.id.text_wifi_connect:
			{
				
			}
				break;
		}
		
	}
	
	private void SendMsg(String ip, int port, String msg) throws UnknownHostException, IOException
	{
		try
		{
			Socket socket = null;
			socket = new Socket(ip, port);
			BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream()));
			writer.write(msg);
			writer.flush();
			writer.close();
			socket.close();
		}
		catch (UnknownHostException e)
		{
			e.printStackTrace();
		}
		catch (IOException e)
		{
			e.printStackTrace();
		}
	}
	
	class TransThread implements Runnable
	{
		public void run()
		{
			try
			{
				// InetAddress serverAddr = InetAddress.getByName(dev_ip);
				Log.v(tag, "connecting...");
				
				pic_socket = new Socket(dev_ip, img_port);
			}
			catch (IOException e)
			{
				e.printStackTrace();
				
				return;
			}
			
			if (pic_socket == null)
			{
				Log.e(tag, "connect trans thread failed");
				return;
			}
			
			while (!Thread.currentThread().isInterrupted())
			{
				Message m = Message.obtain();
				m.what = MSG_IMGSHOW;
				
				// InputStream in;
				try
				{
					
					DataInputStream dis = new DataInputStream(pic_socket.getInputStream());
					int picSize = dis.readInt();
					Log.v(tag, "picLen = " + picSize);
					byte[] data = new byte[picSize];
					
					int len = 0;
					while (len < picSize)
					{
						Log.v(tag, "read data....");
						len += dis.read(data, len, picSize - len);
					}
					Log.v(tag, "start decodes--->");
					
					ByteArrayOutputStream output = new ByteArrayOutputStream();
					bitmap = BitmapFactory.decodeByteArray(data, 0, data.length);
					Log.v(tag, "bitmap value: " + String.valueOf(bitmap));
					bitmap.compress(CompressFormat.JPEG, 100, output);
					bmpHandler.sendMessage(m);
					
					String ack = "got it!";
					DataOutputStream dos = new DataOutputStream(pic_socket.getOutputStream());
					dos.writeChars(ack);
					dos.flush();
					
				}
				catch (IOException e)
				{
					if (interrupttime < 50)
					{
						e.printStackTrace();
						interrupttime++;
					}
					else
					{
						Thread.currentThread().interrupt();
						Log.e(tag, "stop thread...");
					}
					
				}
				
			}
		}
	}
	
	@Override
	public void onStop()
	{
		super.onStop();
	}
	
	@Override
	public void onResume()
	{
		super.onResume();
	}
	
	@Override
	public void onPause()
	{
		// mSensorManager.unregisterListener(GrivatySeListener, sensor);
		mSensorManager.unregisterListener(GrivatySeListener);
		super.onPause();
	}
	
	@Override
	public void onDestroy()
	{
		super.onDestroy();
		
	}
	
}
