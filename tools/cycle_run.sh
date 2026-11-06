#!/bin/bash
# Cycling test (docs/TESTING.md): all 7 screens every 10 s for $1 minutes, /api/status polled every 2 s, serial logged.
# Reports restarts (uptime going back), aborts, refusals, LVGL peak; restores the owner's settings.
# Usage: QRP_ADMIN_PW=... QRP_IP=<ip> QRP_SETTINGS=<saved.json> tools/cycle_run.sh <minutes>
# Exit 0 when there was no restart and no abort. Download results: grep the log for "[Mem] job".
MIN=$1; A="--digest -u admin:$QRP_ADMIN_PW"; IP=${QRP_IP:?set QRP_IP to the device address}; P=$(ls /dev/ttyUSB* | head -1); LOG=${QRP_LOG_DIR:-release/logs}/cycle-$MIN.log; SAVED=${QRP_SETTINGS:?set QRP_SETTINGS to a JSON file with the settings to restore (cycle_en, cycle_screens, cycle_s, cycle_order, timeout)}
stty -F $P 115200 -hupcl raw -echo; (timeout $((MIN*60+60)) cat $P > $LOG 2>&1 &)
curl -s -m 10 $A -X POST -H "Content-Type: application/json" -d '{"cycle_en":true,"cycle_screens":127,"cycle_s":10,"cycle_order":"0,1,2,3,4,5,6","timeout":0}' -o /dev/null http://$IP/api/config/save
prev=0; restarts=0; ok=0; miss=0; end=$(( $(date +%s) + MIN*60 ))
while [ $(date +%s) -lt $end ]; do
  u=$(curl -s -m 5 $A http://$IP/api/status | python3 -c "import json,sys; print(json.load(sys.stdin)['uptime'])" 2>/dev/null)
  if [ -n "$u" ]; then ok=$((ok+1)); [ "$u" -lt "$prev" ] && restarts=$((restarts+1)); prev=$u; else miss=$((miss+1)); fi
  sleep 2
done
for i in 1 2 3 4 5; do curl -s -m 10 $A -X POST -H "Content-Type: application/json" -d "$(cat "$SAVED")" -o /dev/null -w "%{http_code}" http://$IP/api/config/save | grep -q 200 && { echo "settings restored"; break; }; sleep 10; done
sleep 60
m=$(curl -s -m 8 $A http://$IP/api/status | python3 -c "import json,sys; d=json.load(sys.stdin); m=d['mem']; print('uptime',d['uptime'],'min_free',m['heap_min_free'],'largest',m['largest_block'],'largest_min',m['largest_block_min'],'lvgl_max',m['lvgl_max_used'],'refused',m['web_refused'],'stacks',m['stack_free'])" 2>/dev/null)
cause=$(tr -cd '\11\12\15\40-\176' < $LOG | grep -m1 -E "abort\(\) was called|Guru Meditation|panic" )
echo "cycling $MIN min: polls ok $ok, no answer $miss, restarts $restarts | ${cause:-no abort}"
echo "after: $m"
tr -cd '\11\12\15\40-\176' < $LOG | grep -E "\[Mem\] job" | tail -8
[ $restarts = 0 ] && [ -z "$cause" ]
