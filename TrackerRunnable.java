public class TrackerRunnable implements Runnable{

	
	
	static {
		System.loadLibrary("TrackerRunnable");
	}

	// TODO : remover
	public static void main(String[] args) {
		//new TrackerRunnable().doTracker();
	}

	private void registerUser(int id, String name, float confidence) {
		UserTrackerManager.registerUser(id, name, confidence);
		System.out.println("JAVA diz: +++++++++++> Nova label para : " + id + " - " + name + " - " + confidence + ".");
	}

	@Override
	public void run() {
	}

	private class Run{
		private native void doTracker();
	}

}
