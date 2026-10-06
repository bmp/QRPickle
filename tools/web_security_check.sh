#!/usr/bin/env bash
# Black-box security checks against a running QRPickle web console (review area 1).
# Usage: QRP_ADMIN_PW=<password> tools/web_security_check.sh [device-ip]
# The password is on the device's Network screen and in the boot log.
set -u
IP="${1:-192.168.0.6}"; B="http://$IP"; PW="${QRP_ADMIN_PW:?set QRP_ADMIN_PW}"
pass=0; fail=0
check() { if [ "$2" = "$3" ]; then echo "PASS  $1"; pass=$((pass+1)); else echo "FAIL  $1 (got $2, want $3)"; fail=$((fail+1)); fi; }
code() { curl -s -o /dev/null -m 10 -w '%{http_code}' "$@"; }
auth=(--digest -u "admin:$PW")

check "GET / requires auth"                      "$(code "$B/")" 401
check "GET /api/config requires auth"            "$(code "$B/api/config")" 401
check "wrong password rejected"                  "$(code --digest -u admin:wrong-pw "$B/api/config")" 401
check "POST /api/config/save requires auth"      "$(code -X POST -H 'Content-Type: application/json' -d '{"callsign":"N0BAD"}' "$B/api/config/save")" 401
check "POST /api/system/reboot requires auth"    "$(code -X POST "$B/api/system/reboot")" 401
check "POST /api/system/update requires auth"    "$(code -X POST -F 'f=@/dev/null' "$B/api/system/update?target=firmware")" 401
check "POST /api/cloud_ota/flash requires auth"  "$(code -X POST "$B/api/cloud_ota/flash")" 401
check "GET /api/config with auth"                "$(code "${auth[@]}" "$B/api/config")" 200

cfg=$(curl -s -m 10 "${auth[@]}" "$B/api/config")
check "secrets not returned (password empty)"    "$(echo "$cfg" | python3 -c 'import sys,json;d=json.load(sys.stdin);print(all(d.get(k,"")=="" for k in ("password","apikey","aprs_pass","hamalert_pass")))')" True
check "set-flags present"                        "$(echo "$cfg" | python3 -c 'import sys,json;d=json.load(sys.stdin);print("password_set" in d and "apikey_set" in d)')" True
# --digest shows the 401 challenge and the 200 response headers; CSP must be on the final one.
check "CSP header present"                       "$(curl -s -m 10 -D - -o /dev/null "${auth[@]}" "$B/api/status" | grep -ci '^content-security-policy:' | awk '{print ($1>0)?"yes":"no"}')" yes

up1=$(curl -s -m 10 "${auth[@]}" "$B/api/status" | python3 -c 'import sys,json;print(json.load(sys.stdin)["uptime"])')
check "non-string JSON accepted without crash"   "$(code "${auth[@]}" -X POST -H 'Content-Type: application/json' -d '{"callsign":5,"grid":[1],"password":7}' "$B/api/config/save")" 200
sleep 3
up2=$(curl -s -m 10 "${auth[@]}" "$B/api/status" | python3 -c 'import sys,json;print(json.load(sys.stdin)["uptime"])' 2>/dev/null || echo 0)
check "device did not reboot after bad JSON"     "$([ "${up2:-0}" -ge "${up1:-0}" ] && echo yes || echo no)" yes
check "path-traversal profile name rejected"     "$(code "${auth[@]}" "$B/api/profiles/get?name=../www/app")" 404
check "markup profile name rejected"             "$(code "${auth[@]}" -X POST -H 'Content-Type: application/json' -d '{"name":"<img src=x>","config":{}}' "$B/api/profiles/save")" 400
echo "RESULT pass=$pass fail=$fail"; [ "$fail" -eq 0 ]
