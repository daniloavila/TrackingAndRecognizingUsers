import java.util.HashMap;
import java.util.Map;

public class UserTrackerManager {

	static {
		System.loadLibrary("TrackerRunnable");
	}
	
	private static Thread tracker;
	private static Map<Integer, User> labels;

	public static void initUserTracker() {
		if (tracker == null) {
			labels = new HashMap<Integer, UserTrackerManager.User>();
			tracker = new Thread(new TrackerRunnable());
			tracker.start();
		}
	}

	public static void registerUser(int id, String name, float confidence) {
		labels.put(id, new User(name, confidence));
	}

	// TODO : remover
	public static void main(String[] args) {
		UserTrackerManager.initUserTracker();
	}
	
	private static class User {
		String name;
		Float confidence;
		
		User(String name, Float confidence) {
			this.name = name;
			this.confidence = confidence;
		}
	}

}

