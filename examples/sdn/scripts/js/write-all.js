/* 600000 ms -> 10min */
/* 1800000 ms -> 30min */
/* 3600000 ms -> 1h */
TIMEOUT(3600000); /* milliseconds. print last msg at timeout */

var file = "/home/mike/Results/" + mote.getSimulation().getTitle() + ".log";

// Write over old file
log.writeFile(file, "")

while (true) {
  try {
    YIELD();
    s_time = String("           " + time).slice(-11);
    s_id = String("   " + id).slice(-3);
    log.log(s_time + ": " + s_id + ": " + msg + "\n");
    log.append(file, s_time + ": " + s_id + ": " + msg + "\n");
  } catch (e) {
    throw('test script killed');
  }
}
