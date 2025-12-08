#!/bin/bash

# Integration test script for nimd server

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# random high port to avoid conflicts
PORT=$((10000 + RANDOM % 40000))
SERVER_PID=""
TEST_LOG="/tmp/nimd_test_${PORT}.log"

cleanup() {
  echo -e "\n${BLUE}=== Cleaning up ===${NC}"
  if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "Killing server (PID: $SERVER_PID)"
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi

  pkill -f "rawc localhost $PORT" 2>/dev/null || true
  pkill -f "testc localhost $PORT" 2>/dev/null || true

  rm -f "$TEST_LOG"
  rm -f /tmp/*_${PORT}.txt

  echo -e "\n${BLUE}=== Test Summary ===${NC}"
  echo -e "Tests run:    $TESTS_RUN"
  echo -e "${GREEN}Tests passed: $TESTS_PASSED${NC}"
  if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Tests failed: $TESTS_FAILED${NC}"
  else
    echo -e "Tests failed: $TESTS_FAILED"
  fi

  if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
  fi
  exit 0
}

trap cleanup EXIT INT TERM

print_test() {
  echo -e "\n${BLUE}TEST $1: $2${NC}"
  TESTS_RUN=$((TESTS_RUN + 1))
}

pass() {
  echo -e "${GREEN}PASS${NC}: $1"
  TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
  echo -e "${RED}FAIL${NC}: $1"
  TESTS_FAILED=$((TESTS_FAILED + 1))
}

send_message() {
  local msg="$1"
  local timeout_val="${2:-2}"
  echo -n "$msg" | timeout "$timeout_val" ./clients/src/rawc localhost "$PORT" 2>&1
}

wait_for_server() {
  local max_attempts=20
  local attempt=0

  while [ $attempt -lt $max_attempts ]; do
    if nc -z localhost "$PORT" 2>/dev/null; then
      echo "Server ready on port $PORT"
      sleep 0.5
      return 0
    fi
    sleep 0.1
    attempt=$((attempt + 1))
  done

  echo "Server failed to start on port $PORT"
  return 1
}

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Nim Server Integration Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"

echo -e "\n${YELLOW}Building server...${NC}"
if ! make regular 2>&1 | tee -a "$TEST_LOG"; then
  echo -e "${RED}Failed to build server${NC}"
  exit 1
fi

echo -e "\n${YELLOW}Building client tools...${NC}"
cd clients/src
if ! make 2>&1 | tee -a "$TEST_LOG"; then
  echo -e "${RED}Failed to build clients${NC}"
  exit 1
fi
cd ../..

if [ ! -x "./nimd" ]; then
  echo -e "${RED}Server executable './nimd' not found${NC}"
  exit 1
fi

if [ ! -x "./clients/src/rawc" ]; then
  echo -e "${RED}Client executable './clients/src/rawc' not found${NC}"
  exit 1
fi

echo -e "\n${YELLOW}Starting server on port $PORT...${NC}"
./nimd "$PORT" >"$TEST_LOG" 2>&1 &
SERVER_PID=$!

echo "Server PID: $SERVER_PID"

if ! wait_for_server; then
  echo -e "${RED}Server failed to start${NC}"
  exit 1
fi

sleep 0.5

echo -e "\n${BLUE}========================================${NC}"
echo -e "${BLUE}  Running Tests${NC}"
echo -e "${BLUE}========================================${NC}"

print_test "1.1" "Client connects and sends valid OPEN message"
RESPONSE=$(send_message "0|11|OPEN|Alice|")
if echo "$RESPONSE" | grep -q "0|05|WAIT|"; then
  pass "Server responded with WAIT message"
else
  fail "Expected WAIT response, got: $RESPONSE"
fi

print_test "1.2" "Server sends correct message length"
# Content: "OPEN|Bob|" = 9 bytes
RESPONSE=$(send_message "0|09|OPEN|Bob|")
if echo "$RESPONSE" | grep -q "0|05|WAIT|"; then
  pass "WAIT message has correct length (05)"
else
  fail "WAIT message length incorrect: $RESPONSE"
fi

print_test "2.1" "Invalid version number"
RESPONSE=$(send_message "8|11|OPEN|Alice|")
if echo "$RESPONSE" | grep -q "FAIL" && echo "$RESPONSE" | grep -q "10 Invalid"; then
  pass "Server rejected invalid version with FAIL 10 Invalid"
else
  fail "Expected FAIL 10 Invalid for bad version, got: $RESPONSE"
fi

print_test "2.2" "Invalid message length format"
RESPONSE=$(send_message "0|XX|OPEN|Alice|")
if echo "$RESPONSE" | grep -q "FAIL" || [ -z "$RESPONSE" ]; then
  pass "Server rejected invalid length format"
else
  fail "Server should reject non-numeric length: $RESPONSE"
fi

print_test "2.3" "Message length too short"
RESPONSE=$(send_message "0|03|OPEN|Alice|")
if echo "$RESPONSE" | grep -q "FAIL.*10 Invalid"; then
  pass "Server rejected message with length < 5"
else
  fail "Expected FAIL for length < 5, got: $RESPONSE"
fi

print_test "2.4" "Unknown message type"
RESPONSE=$(send_message "0|11|FAKE|Alice|")
if echo "$RESPONSE" | grep -q "FAIL.*10 Invalid"; then
  pass "Server rejected unknown message type"
else
  fail "Expected FAIL for unknown message type, got: $RESPONSE"
fi

print_test "2.5" "Message missing required fields"
RESPONSE=$(send_message "0|05|OPEN|")
if echo "$RESPONSE" | grep -q "FAIL.*10 Invalid"; then
  pass "Server rejected message missing required fields"
else
  fail "Expected FAIL for missing fields, got: $RESPONSE"
fi

print_test "2.6" "Message length mismatch (stated length too long)"
RESPONSE=$(send_message "0|20|OPEN|Bob|")
if echo "$RESPONSE" | grep -q "FAIL.*10 Invalid"; then
  pass "Server rejected message with length mismatch"
else
  fail "Expected FAIL for length mismatch, got: $RESPONSE"
fi

print_test "2.7" "MOVE with non-digit fields"
RESPONSE=$(send_message "0|09|MOVE|a|b|")
if echo "$RESPONSE" | grep -q "FAIL.*10 Invalid"; then
  pass "Server rejected MOVE with non-digit fields"
else
  fail "Expected FAIL 10 Invalid for non-digit MOVE, got: $RESPONSE"
fi

print_test "3.0" "Player name exactly 72 characters (boundary - should succeed)"
NAME_72=$(printf '%72s' | tr ' ' 'A')
RESPONSE=$(send_message "0|78|OPEN|${NAME_72}|")
if echo "$RESPONSE" | grep -q "WAIT"; then
  pass "Server accepted exactly 72-char name"
else
  fail "Server rejected valid 72-char name: $RESPONSE"
fi

print_test "3.1" "Player name exceeds 72 characters"
LONG_NAME=$(printf '%81s' | tr ' ' 'B')
RESPONSE=$(send_message "0|87|OPEN|${LONG_NAME}|")
if echo "$RESPONSE" | grep -q "FAIL.*21 Long Name"; then
  pass "Server rejected name > 72 characters with FAIL 21"
else
  fail "Expected FAIL 21 Long Name, got: $RESPONSE"
fi

print_test "3.2" "Client sends OPEN twice"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|11|OPEN|Alice|" >&3
  sleep 0.3
  read -t 1 -u 3 RESP1 || true
  echo "$RESP1" >/tmp/open1_${PORT}.txt

  echo -n "0|11|OPEN|Alice|" >&3
  sleep 0.3
  read -t 1 -u 3 RESP2 || true
  echo "$RESP2" >/tmp/open2_${PORT}.txt
  exec 3>&-
) 2>/dev/null

RESP2=$(cat /tmp/open2_${PORT}.txt 2>/dev/null || echo "")

if echo "$RESP2" | grep -q "23 Already Open"; then
  pass "Server rejected duplicate OPEN with FAIL 23"
else
  fail "Expected FAIL 23 Already Open, got: $RESP2"
fi

rm -f /tmp/open1_${PORT}.txt /tmp/open2_${PORT}.txt

print_test "3.3" "Client sends MOVE before NAME"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|10|OPEN|Dave|" >&3
  sleep 0.3
  read -t 1 -u 3 RESP1 || true

  echo -n "0|09|MOVE|1|3|" >&3
  sleep 0.3
  read -t 1 -u 3 RESP2 || true
  echo "$RESP2" >/tmp/move_before_${PORT}.txt
  exec 3>&-
) 2>/dev/null

RESP2=$(cat /tmp/move_before_${PORT}.txt 2>/dev/null || echo "")

if echo "$RESP2" | grep -q "24 Not Playing"; then
  pass "Server rejected MOVE before NAME with FAIL 24"
else
  fail "Expected FAIL 24 Not Playing, got: $RESP2"
fi

rm -f /tmp/move_before_${PORT}.txt

print_test "3.4" "Empty player name"
RESPONSE=$(send_message "0|06|OPEN||")
if echo "$RESPONSE" | grep -q "FAIL"; then
  pass "Server rejected empty name"
else
  fail "Server should reject empty name: $RESPONSE"
fi

print_test "4.1" "Two clients connect and get matched"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|Player1|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  echo "CLIENT1_WAIT: $RESP1" >/tmp/client1_${PORT}.txt
  read -t 3 -u 3 RESP2 || true # NAME
  echo "CLIENT1_NAME: $RESP2" >>/tmp/client1_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 0.5

(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|Player2|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  echo "CLIENT2_WAIT: $RESP1" >/tmp/client2_${PORT}.txt
  read -t 3 -u 3 RESP2 || true # NAME
  echo "CLIENT2_NAME: $RESP2" >>/tmp/client2_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID2=$!

wait $PID1 2>/dev/null || true
wait $PID2 2>/dev/null || true
sleep 0.5

CLIENT1_RESP=$(cat /tmp/client1_${PORT}.txt 2>/dev/null || echo "")
CLIENT2_RESP=$(cat /tmp/client2_${PORT}.txt 2>/dev/null || echo "")

if echo "$CLIENT1_RESP" | grep -q "NAME" && echo "$CLIENT2_RESP" | grep -q "NAME"; then
  pass "Both clients received NAME messages after matching"
else
  fail "Clients did not receive NAME messages. C1: $CLIENT1_RESP, C2: $CLIENT2_RESP"
fi

rm -f /tmp/client1_${PORT}.txt /tmp/client2_${PORT}.txt

print_test "4.2" "NAME messages contain opponent names"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|13|OPEN|Charlie|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  echo "$RESP2" >/tmp/client1_name_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 0.5

(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|11|OPEN|Delta|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  echo "$RESP2" >/tmp/client2_name_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID2=$!

wait $PID1 2>/dev/null || true
wait $PID2 2>/dev/null || true
sleep 0.5

CLIENT1_NAME=$(cat /tmp/client1_name_${PORT}.txt 2>/dev/null || echo "")
CLIENT2_NAME=$(cat /tmp/client2_name_${PORT}.txt 2>/dev/null || echo "")

# Client 1 should receive opponent name "Delta"
# Client 2 should receive opponent name "Charlie"
if echo "$CLIENT1_NAME" | grep -q "Delta" && echo "$CLIENT2_NAME" | grep -q "Charlie"; then
  pass "NAME messages contain correct opponent names"
else
  fail "NAME messages missing opponent names. C1: $CLIENT1_NAME, C2: $CLIENT2_NAME"
fi

rm -f /tmp/client1_name_${PORT}.txt /tmp/client2_name_${PORT}.txt

print_test "4.3" "First PLAY message sent with initial board state"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|10|OPEN|Echo|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  echo "$RESP3" >/tmp/client1_play_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 0.5

(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|13|OPEN|Foxtrot|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  echo "$RESP3" >/tmp/client2_play_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID2=$!

wait $PID1 2>/dev/null || true
wait $PID2 2>/dev/null || true
sleep 0.5

CLIENT1_PLAY=$(cat /tmp/client1_play_${PORT}.txt 2>/dev/null || echo "")
CLIENT2_PLAY=$(cat /tmp/client2_play_${PORT}.txt 2>/dev/null || echo "")

# Both should receive PLAY with board "1 3 5 7 9"
if (echo "$CLIENT1_PLAY" | grep -q "PLAY" && echo "$CLIENT1_PLAY" | grep -q "1 3 5 7 9") ||
  (echo "$CLIENT2_PLAY" | grep -q "PLAY" && echo "$CLIENT2_PLAY" | grep -q "1 3 5 7 9"); then
  pass "PLAY message contains initial board state (1 3 5 7 9)"
else
  fail "PLAY message missing correct board state. C1: $CLIENT1_PLAY, C2: $CLIENT2_PLAY"
fi

rm -f /tmp/client1_play_${PORT}.txt /tmp/client2_play_${PORT}.txt

print_test "5.1" "Cannot connect with same name as active player"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|14|OPEN|SameName|" >&3
  read -t 2 -u 3 RESP1 || true
  echo "FIRST: $RESP1" >/tmp/first_same_${PORT}.txt
  sleep 3
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 1

RESPONSE=$(send_message "0|14|OPEN|SameName|")
if echo "$RESPONSE" | grep -q "22 Already Playing"; then
  pass "Server rejected duplicate name with FAIL 22"
else
  fail "Expected FAIL 22 Already Playing, got: $RESPONSE"
fi

kill $PID1 2>/dev/null || true
wait $PID1 2>/dev/null || true
rm -f /tmp/first_same_${PORT}.txt

print_test "6.1" "Invalid move - pile index out of range"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|MoveTest1|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  echo -n "0|09|MOVE|5|1|" >&3
  read -t 2 -u 3 RESP4 || true
  echo "$RESP4" >/tmp/pile_resp_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 0.5

(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|MoveTest2|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  sleep 2
  exec 3>&-
) 2>/dev/null &
PID2=$!

wait $PID1 2>/dev/null || true
wait $PID2 2>/dev/null || true

PILE_RESP=$(cat /tmp/pile_resp_${PORT}.txt 2>/dev/null || echo "")

if echo "$PILE_RESP" | grep -q "FAIL.*32 Pile Index"; then
  pass "Server rejected invalid pile index with FAIL 32"
else
  fail "Expected FAIL 32 Pile Index, got: $PILE_RESP"
fi

rm -f /tmp/pile_resp_${PORT}.txt

print_test "6.2" "Invalid move - quantity too large"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|QtyTest1|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  echo -n "0|09|MOVE|0|5|" >&3
  read -t 2 -u 3 RESP4 || true
  echo "$RESP4" >/tmp/qty_resp_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 0.5

(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|QtyTest2|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  sleep 2
  exec 3>&-
) 2>/dev/null &
PID2=$!

wait $PID1 2>/dev/null || true
wait $PID2 2>/dev/null || true

QTY_RESP=$(cat /tmp/qty_resp_${PORT}.txt 2>/dev/null || echo "")

if echo "$QTY_RESP" | grep -q "FAIL.*33 Quantity"; then
  pass "Server rejected invalid quantity with FAIL 33"
else
  fail "Expected FAIL 33 Quantity, got: $QTY_RESP"
fi

rm -f /tmp/qty_resp_${PORT}.txt

print_test "6.3" "Invalid move - zero stones"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|ZeroTest1|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  echo -n "0|09|MOVE|2|0|" >&3
  read -t 2 -u 3 RESP4 || true
  echo "$RESP4" >/tmp/zero_resp_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 0.5

(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|ZeroTest2|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  sleep 2
  exec 3>&-
) 2>/dev/null &
PID2=$!

wait $PID1 2>/dev/null || true
wait $PID2 2>/dev/null || true

ZERO_RESP=$(cat /tmp/zero_resp_${PORT}.txt 2>/dev/null || echo "")

if echo "$ZERO_RESP" | grep -q "FAIL.*33 Quantity"; then
  pass "Server rejected zero quantity with FAIL 33"
else
  fail "Expected FAIL 33 Quantity, got: $ZERO_RESP"
fi

rm -f /tmp/zero_resp_${PORT}.txt

print_test "6.4" "Invalid move - negative pile index"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|NegTest1|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  echo -n "0|10|MOVE|-1|1|" >&3
  read -t 2 -u 3 RESP4 || true
  echo "$RESP4" >/tmp/neg_resp_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 0.5

(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|NegTest2|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  sleep 2
  exec 3>&-
) 2>/dev/null &
PID2=$!

wait $PID1 2>/dev/null || true
wait $PID2 2>/dev/null || true

NEG_RESP=$(cat /tmp/neg_resp_${PORT}.txt 2>/dev/null || echo "")

if echo "$NEG_RESP" | grep -q "FAIL.*\(32 Pile Index\|10 Invalid\)"; then
  pass "Server rejected negative pile index"
else
  fail "Expected FAIL for negative pile index, got: $NEG_RESP"
fi

rm -f /tmp/neg_resp_${PORT}.txt

print_test "7.1" "Client disconnects before matching"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|15|OPEN|Quitter1|" >&3
  sleep 0.3
  exec 3>&-
) 2>/dev/null &
wait $! 2>/dev/null || true

sleep 0.5

RESPONSE=$(send_message "0|15|OPEN|NewPlayer|")
if echo "$RESPONSE" | grep -q "WAIT"; then
  pass "Server continues accepting connections after client disconnect"
else
  fail "Server may have crashed after disconnect: $RESPONSE"
fi

print_test "7.2" "Server handles abrupt disconnection gracefully"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|12|OPEN|Abrupt|" >&3
  exec 3>&-
) 2>/dev/null &
wait $! 2>/dev/null || true

sleep 0.5

RESPONSE=$(send_message "0|16|OPEN|StillAlive|")
if echo "$RESPONSE" | grep -q "WAIT"; then
  pass "Server survived abrupt disconnection"
else
  fail "Server may be unresponsive after abrupt disconnect"
fi

print_test "7.3" "Opponent receives forfeit on disconnect"
(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|14|OPEN|Stayer|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 5 -u 3 RESP2 || true # NAME
  read -t 3 -u 3 RESP3 || true # PLAY
  read -t 5 -u 3 RESP4 || true # OVER
  echo "$RESP4" >/tmp/forfeit_resp_${PORT}.txt
  exec 3>&-
) 2>/dev/null &
PID1=$!

sleep 0.5

(
  exec 3<>/dev/tcp/localhost/$PORT
  echo -n "0|14|OPEN|Leaver|" >&3
  read -t 2 -u 3 RESP1 || true # WAIT
  read -t 3 -u 3 RESP2 || true # NAME
  exec 3>&-
) 2>/dev/null &
PID2=$!

wait $PID1 2>/dev/null || true
wait $PID2 2>/dev/null || true

FORFEIT_RESP=$(cat /tmp/forfeit_resp_${PORT}.txt 2>/dev/null || echo "")

if echo "$FORFEIT_RESP" | grep -q "OVER" && echo "$FORFEIT_RESP" | grep -q "Forfeit"; then
  pass "Remaining player received OVER with Forfeit"
else
  fail "Expected OVER with Forfeit, got: $FORFEIT_RESP"
fi

rm -f /tmp/forfeit_resp_${PORT}.txt

print_test "8.1" "Standard message handling"
RESPONSE=$(send_message "0|11|OPEN|Alice|")
if echo "$RESPONSE" | grep -q "WAIT"; then
  pass "Server handles standard message correctly"
else
  fail "Server failed on standard message: $RESPONSE"
fi

print_test "8.2" "Message with maximum content length (99 bytes)"
NAME_93=$(printf '%93s' | tr ' ' 'X')
RESPONSE=$(send_message "0|99|OPEN|${NAME_93}|")
if echo "$RESPONSE" | grep -q "FAIL.*21 Long Name"; then
  pass "Server correctly rejected 93-char name with FAIL 21"
else
  fail "Expected FAIL 21 Long Name for 93-char name, got: $RESPONSE"
fi

print_test "8.3" "Message exceeding maximum length"
NAME_100=$(printf '%100s' | tr ' ' 'Y')
RESPONSE=$(send_message "0|99|OPEN|${NAME_100}|")
if echo "$RESPONSE" | grep -q "FAIL"; then
  pass "Server rejected oversized message"
else
  fail "Server should reject oversized message: $RESPONSE"
fi

echo -e "\n${BLUE}==============================${NC}"
echo -e "${BLUE}  Test Run Complete${NC}"
echo -e "${BLUE}==============================${NC}"1
