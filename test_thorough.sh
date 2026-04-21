#!/bin/bash
# Thorough ft_irc eval test suite
cd /mnt/c/Users/Manu/Desktop/ft_irc
killall ircserv 2>/dev/null; sleep 0.3
./ircserv 8888 pass > /tmp/srv.log 2>&1 &
SRV_PID=$!
sleep 0.5

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; NC='\033[0m'
pass=0; fail=0

header() { echo ""; echo -e "${YELLOW}=== $1 ===${NC}"; }
check() {
    local desc="$1" out="$2" expect="$3"
    if echo "$out" | grep -qE "$expect"; then
        echo -e "  ${GREEN}[PASS]${NC} $desc"; pass=$((pass+1))
    else
        echo -e "  ${RED}[FAIL]${NC} $desc  (expected: $expect)"
        echo "         got: $(echo "$out" | tr '\r\n' '||' | cut -c1-180)"
        fail=$((fail+1))
    fi
}
check_not() {
    local desc="$1" out="$2" expect="$3"
    if echo "$out" | grep -qE "$expect"; then
        echo -e "  ${RED}[FAIL]${NC} $desc  (should NOT match: $expect)"
        fail=$((fail+1))
    else
        echo -e "  ${GREEN}[PASS]${NC} $desc"; pass=$((pass+1))
    fi
}

# Send commands via a fresh nc, capture all output
nc_cmd() { printf "$1" | nc -w 2 127.0.0.1 8888 2>/dev/null; }

# Open a persistent connection to a tmp file, return fd
# Usage: open_conn <fd> <nick>
open_conn() {
    local fd=$1 nick=$2
    eval "exec ${fd}<>/dev/tcp/127.0.0.1/8888"
    printf "PASS pass\r\nNICK ${nick}\r\nUSER ${nick}u 0 * :${nick}\r\n" >&$fd
    sleep 0.4
}
close_conn() { eval "exec ${1}>&-"; sleep 0.3; }

# Read output accumulated on fd into variable, with short timeout
read_fd() {
    local fd=$1 secs=${2:-0.4}
    timeout $secs cat <&$fd 2>/dev/null
}

# Send to fd and wait
send_fd() { printf "$2" >&$1; sleep ${3:-0.3}; }

# ─── SECTION 1: Registration ──────────────────────────────────────────────────
header "Registration"

OUT=$(nc_cmd "PASS pass\r\nNICK alice\r\nUSER aliceu 0 * :alice\r\n")
check "PASS/NICK/USER registers" "$OUT" "001 alice"
check "Welcome message sent" "$OUT" "Welcome to ft_irc"
check "MOTD shown" "$OUT" "HOW TO GET STARTED"
check "Auto-joined #general" "$OUT" "auto-joined to #general"
check "001 has correct server prefix" "$OUT" ":ft_irc 001"

OUT=$(nc_cmd "PASS badpass\r\n")
check "Wrong password → 464" "$OUT" "464"
check "Wrong password → ERROR close" "$OUT" "ERROR :Closing Link"

OUT=$(nc_cmd "PASS\r\n")
check "PASS with no arg → 461, stay connected" "$OUT" "461 PASS"
check_not "PASS with no arg → no ERROR disconnect" "$OUT" "ERROR"

OUT=$(nc_cmd "NICK alice\r\n")
check "NICK before PASS → rejected (464 or 451)" "$OUT" "46[14]"

OUT=$(nc_cmd "PASS pass\r\nNICK bad@nick\r\n")
check "Nick with @ → 432" "$OUT" "432"

OUT=$(nc_cmd "PASS pass\r\nNICK 9digits\r\n")
check "Nick starting with digit → 432" "$OUT" "432"

OUT=$(nc_cmd "PASS pass\r\nNICK toolongnick\r\n")
check "Nick > 9 chars → 432" "$OUT" "432"

OUT=$(nc_cmd "PASS pass\r\nNICK ok-Nick\r\n")
check_not "Valid nick (ok-Nick) → no 432" "$OUT" "432"

# ─── SECTION 2: Duplicate nick ────────────────────────────────────────────────
header "Duplicate Nick"
open_conn 3 dup1; sleep 0.2
OUT=$(nc_cmd "PASS pass\r\nNICK dup1\r\nUSER dup1u 0 * :dup1\r\n")
check "Duplicate nick → 433" "$OUT" "433"
check_not "Duplicate nick → not registered" "$OUT" "001"
close_conn 3

# ─── SECTION 3: Channel join & messaging ──────────────────────────────────────
header "Channel Basics"
open_conn 3 ca; open_conn 4 cb
send_fd 3 "JOIN #ch1\r\n"
send_fd 4 "JOIN #ch1\r\n"
send_fd 3 "PRIVMSG #ch1 :hello ch1\r\n"
sleep 0.4
B_OUT=$(read_fd 4)
A_OUT=$(read_fd 3)
check "Channel PRIVMSG received by member" "$B_OUT" "hello ch1"
check "Channel PRIVMSG has nick!user format" "$B_OUT" "ca!cau@localhost PRIVMSG #ch1"
check_not "Sender does NOT get own channel msg" "$A_OUT" "hello ch1"
close_conn 3; close_conn 4

# ─── SECTION 4: PRIVMSG user-to-user ─────────────────────────────────────────
header "Private Messages (user-to-user)"
open_conn 3 pma; open_conn 4 pmb; open_conn 5 pmc
send_fd 3 "JOIN #pmch\r\n"
send_fd 3 "PRIVMSG pmb :secret\r\n"
sleep 0.4
B_OUT=$(read_fd 4)
C_OUT=$(read_fd 5)
check "User-to-user PRIVMSG delivered" "$B_OUT" "secret"
check_not "Non-target does NOT receive PM" "$C_OUT" "secret"

# PRIVMSG to channel — pmc not in channel
send_fd 3 "PRIVMSG #pmch :chan only\r\n"; sleep 0.4
C_OUT2=$(read_fd 5)
check_not "Non-member does NOT get channel PRIVMSG" "$C_OUT2" "chan only"

# PRIVMSG to nonexistent nick
send_fd 3 "PRIVMSG ghost :hi\r\n"; sleep 0.3
A_OUT=$(read_fd 3)
check "PRIVMSG to nonexistent nick → 401" "$A_OUT" "401"
close_conn 3; close_conn 4; close_conn 5

# ─── SECTION 5: KICK ──────────────────────────────────────────────────────────
header "KICK"
open_conn 3 kop; open_conn 4 kusr
send_fd 3 "JOIN #kroom\r\n"
send_fd 4 "JOIN #kroom\r\n"

# Non-op tries to kick
send_fd 4 "KICK #kroom kop :try\r\n"; sleep 0.3
USR=$(read_fd 4)
check "Non-op KICK → 482" "$USR" "482"

# Op kicks non-op
send_fd 3 "KICK #kroom kusr :bye\r\n"; sleep 0.4
OP=$(read_fd 3); VIC=$(read_fd 4)
check "Op KICK succeeds → KICK broadcast" "$OP" "KICK #kroom kusr"
check "Kicked user receives KICK msg" "$VIC" "KICK #kroom kusr"

# Kick ghost
send_fd 3 "KICK #kroom ghost :none\r\n"; sleep 0.3
OP2=$(read_fd 3)
check "KICK non-existent nick → 401" "$OP2" "401"

# KICK without channel arg (silently ignored, no crash)
send_fd 3 "KICK kusr :no chan\r\n"; sleep 0.3
OP3=$(read_fd 3)
check_not "KICK without channel → no ERROR crash" "$OP3" "ERROR"
close_conn 3; close_conn 4

# ─── SECTION 6: TOPIC ─────────────────────────────────────────────────────────
header "TOPIC"
open_conn 3 top; open_conn 4 treg
send_fd 3 "JOIN #troom\r\n"
send_fd 4 "JOIN #troom\r\n"

# View empty topic
send_fd 3 "TOPIC #troom\r\n"; sleep 0.3
O=$(read_fd 3)
check "TOPIC on empty channel → 331" "$O" "331"

# Op sets topic
send_fd 3 "TOPIC #troom :Saiyan Arc\r\n"; sleep 0.3
B=$(read_fd 4)
check "TOPIC change broadcast to members" "$B" "Saiyan Arc"

# View topic
send_fd 4 "TOPIC #troom\r\n"; sleep 0.3
B2=$(read_fd 4)
check "TOPIC view → 332 with text" "$B2" "332.*Saiyan Arc"

# MODE +t restricts topic
send_fd 3 "MODE #troom +t\r\n"; sleep 0.3
send_fd 4 "TOPIC #troom :Naruto\r\n"; sleep 0.3
B3=$(read_fd 4)
check "Non-op TOPIC with +t → 482" "$B3" "482"

# MODE -t allows it again
send_fd 3 "MODE #troom -t\r\n"; sleep 0.3
send_fd 4 "TOPIC #troom :Naruto\r\n"; sleep 0.3
O2=$(read_fd 3)
check "Non-op TOPIC after -t → works" "$O2" "Naruto"
close_conn 3; close_conn 4

# ─── SECTION 7: MODE +i / INVITE ─────────────────────────────────────────────
header "MODE +i and INVITE"
open_conn 3 iop; open_conn 4 iusr; open_conn 5 ireg
send_fd 3 "JOIN #iroom\r\n"
send_fd 3 "MODE #iroom +i\r\n"; sleep 0.3

# Uninvited join blocked
send_fd 4 "JOIN #iroom\r\n"; sleep 0.3
U=$(read_fd 4)
check "JOIN invite-only without invite → 473" "$U" "473"

# Non-op cannot invite
send_fd 5 "JOIN #iroom\r\n"; sleep 0.2  # also blocked
send_fd 5 "INVITE iusr #iroom\r\n"; sleep 0.3
R=$(read_fd 5)
check "Non-op INVITE → 482" "$R" "482"

# Op invites
send_fd 3 "INVITE iusr #iroom\r\n"; sleep 0.3
U2=$(read_fd 4)
check "Invited user gets INVITE notification" "$U2" "INVITE iusr"

# Now invited user can join
send_fd 4 "JOIN #iroom\r\n"; sleep 0.3
U3=$(read_fd 4)
check "Invited user can JOIN +i channel" "$U3" "iusr.*JOIN #iroom"

# Remove +i
send_fd 3 "MODE #iroom -i\r\n"; sleep 0.3
send_fd 5 "JOIN #iroom\r\n"; sleep 0.3
R2=$(read_fd 5)
check "JOIN after -i works without invite" "$R2" "ireg.*JOIN #iroom"
close_conn 3; close_conn 4; close_conn 5

# ─── SECTION 8: MODE +k / channel key ────────────────────────────────────────
header "MODE +k (Channel Key)"
open_conn 3 kop2
send_fd 3 "JOIN #kroom2\r\n"
send_fd 3 "MODE #kroom2 +k pass123\r\n"; sleep 0.3

OUT=$(nc_cmd "PASS pass\r\nNICK ku1\r\nUSER ku1u 0 * :ku1\r\nJOIN #kroom2\r\n")
check "JOIN without key → 475" "$OUT" "475"

OUT=$(nc_cmd "PASS pass\r\nNICK ku2\r\nUSER ku2u 0 * :ku2\r\nJOIN #kroom2 wrong\r\n")
check "JOIN with wrong key → 475" "$OUT" "475"

OUT=$(nc_cmd "PASS pass\r\nNICK ku3\r\nUSER ku3u 0 * :ku3\r\nJOIN #kroom2 pass123\r\n")
check "JOIN with correct key → success" "$OUT" "ku3.*JOIN #kroom2"

# Remove key
send_fd 3 "MODE #kroom2 -k\r\n"; sleep 0.3
OUT=$(nc_cmd "PASS pass\r\nNICK ku4\r\nUSER ku4u 0 * :ku4\r\nJOIN #kroom2\r\n")
check "JOIN without key after -k → success" "$OUT" "ku4.*JOIN #kroom2"
close_conn 3

# ─── SECTION 9: MODE +l / user limit ─────────────────────────────────────────
header "MODE +l (User Limit)"
open_conn 3 lop; open_conn 4 lu1
send_fd 3 "JOIN #lroom\r\n"
send_fd 3 "MODE #lroom +l 2\r\n"; sleep 0.3
send_fd 4 "JOIN #lroom\r\n"; sleep 0.3
LU1=$(read_fd 4)
check "First user joins within limit" "$LU1" "lu1.*JOIN #lroom"

OUT=$(nc_cmd "PASS pass\r\nNICK lu2\r\nUSER lu2u 0 * :lu2\r\nJOIN #lroom\r\n")
check "Third user blocked by +l → 471" "$OUT" "471"

# Remove limit
send_fd 3 "MODE #lroom -l\r\n"; sleep 0.3
OUT=$(nc_cmd "PASS pass\r\nNICK lu3\r\nUSER lu3u 0 * :lu3\r\nJOIN #lroom\r\n")
check "JOIN after -l → success" "$OUT" "lu3.*JOIN #lroom"
close_conn 3; close_conn 4

# ─── SECTION 10: MODE +o / give-take operator ────────────────────────────────
header "MODE +o (Operator Privilege)"
open_conn 3 mop; open_conn 4 musr; open_conn 5 mvic
send_fd 3 "JOIN #mroom\r\n"
send_fd 4 "JOIN #mroom\r\n"
send_fd 5 "JOIN #mroom\r\n"

# musr can't kick yet
send_fd 4 "KICK #mroom mvic :test\r\n"; sleep 0.3
U=$(read_fd 4)
check "Non-op cannot KICK before +o" "$U" "482"

# Give operator to musr
send_fd 3 "MODE #mroom +o musr\r\n"; sleep 0.3
O=$(read_fd 3)
check "MODE +o broadcast to channel" "$O" "MODE #mroom \\+o musr"

# Now musr can kick
send_fd 4 "KICK #mroom mvic :promoted\r\n"; sleep 0.4
U2=$(read_fd 4); V=$(read_fd 5)
check "Promoted user can KICK" "$U2" "KICK #mroom mvic"
check "Kicked user receives KICK" "$V" "KICK #mroom mvic"

# Take operator back
open_conn 6 mvic2
send_fd 6 "JOIN #mroom\r\n"; sleep 0.2
send_fd 3 "MODE #mroom -o musr\r\n"; sleep 0.3
send_fd 4 "KICK #mroom mvic2 :test\r\n"; sleep 0.3
U3=$(read_fd 4)
check "Demoted user cannot KICK" "$U3" "482"
close_conn 3; close_conn 4; close_conn 5; close_conn 6

# ─── SECTION 11: Multiple channels ───────────────────────────────────────────
header "Multiple Channels"
open_conn 3 ma; open_conn 4 mb
send_fd 3 "JOIN #mc1\r\nJOIN #mc2\r\nJOIN #mc3\r\n"; sleep 0.3
send_fd 4 "JOIN #mc1\r\nJOIN #mc3\r\n"; sleep 0.3

send_fd 3 "PRIVMSG #mc1 :msg1\r\n"; sleep 0.3
B=$(read_fd 4)
check "Member receives msg in shared channel" "$B" "msg1"

send_fd 3 "PRIVMSG #mc2 :msg2\r\n"; sleep 0.3
B2=$(read_fd 4)
check_not "Non-member does NOT get msg in other channel" "$B2" "msg2"

# ma is op in #mc1 (created it)
send_fd 3 "KICK #mc1 mb :bye\r\n"; sleep 0.4
O=$(read_fd 3)
check "Creator is op and can kick in own channel" "$O" "KICK #mc1 mb"
close_conn 3; close_conn 4

# ─── SECTION 12: PART ────────────────────────────────────────────────────────
header "PART"
open_conn 3 pa; open_conn 4 pb
send_fd 3 "JOIN #proom\r\n"
send_fd 4 "JOIN #proom\r\n"
send_fd 3 "PART #proom :gone fishing\r\n"; sleep 0.3
B=$(read_fd 4)
check "PART broadcast to channel" "$B" "PART #proom"
check "PART reason included" "$B" "gone fishing"

send_fd 4 "PRIVMSG #proom :after part\r\n"; sleep 0.3
A=$(read_fd 3)
check_not "Parted user does NOT get channel msgs" "$A" "after part"
close_conn 3; close_conn 4

# ─── SECTION 13: QUIT ────────────────────────────────────────────────────────
header "QUIT"
open_conn 3 qa; open_conn 4 qb
send_fd 3 "JOIN #qroom\r\n"
send_fd 4 "JOIN #qroom\r\n"
send_fd 3 "QUIT :power low\r\n"; sleep 0.4
B=$(read_fd 4)
check "QUIT broadcast to channel members" "$B" "QUIT :power low"

OUT=$(nc_cmd "PASS pass\r\nNICK qa\r\nUSER qau 0 * :qa\r\n")
check "Nick freed after QUIT — can re-register" "$OUT" "001 qa"
close_conn 4

# ─── SECTION 14: Partial commands ────────────────────────────────────────────
header "Partial Commands"
OUT=$( (printf 'PASS pass\r\n'; sleep 0.1; printf 'NIC'; sleep 0.1; printf 'K p1\r\n'; sleep 0.1; printf 'USER p1u 0 * :p1\r\n'; sleep 0.3) | nc -w 2 127.0.0.1 8888 2>/dev/null)
check "Partial NICK assembled across packets" "$OUT" "001 p1"

OUT=$( (printf 'PA'; sleep 0.1; printf 'SS '; sleep 0.1; printf 'pass\r\n'; sleep 0.1; printf 'NICK p2\r\nUSER p2u 0 * :p2\r\n'; sleep 0.3) | nc -w 2 127.0.0.1 8888 2>/dev/null)
check "Partial PASS assembled across packets" "$OUT" "001 p2"

OUT=$( (printf 'PASS pass\r\nNICK p3\r\nUSER p3u 0 * :p3\r\nJOIN #ptch\r\nPRIV'; sleep 0.1; printf 'MSG #ptch :hi\r\n'; sleep 0.3) | nc -w 2 127.0.0.1 8888 2>/dev/null)
check "Partial PRIVMSG assembled correctly" "$OUT" "p3.*JOIN #ptch"

# ─── SECTION 15: Client resilience ───────────────────────────────────────────
header "Client Resilience"
open_conn 3 alive; sleep 0.2
send_fd 3 "JOIN #rroom\r\n"; sleep 0.2
open_conn 4 killer; sleep 0.2
send_fd 4 "JOIN #rroom\r\n"; sleep 0.2
close_conn 4  # abrupt kill

sleep 0.3
OUT=$(nc_cmd "PASS pass\r\nNICK newcli\r\nUSER newcliu 0 * :newcli\r\n")
check "Server still accepts new connections after abrupt client kill" "$OUT" "001 newcli"

send_fd 3 "PRIVMSG #rroom :still alive\r\n"; sleep 0.2
O=$(read_fd 3)
check_not "Remaining client has no crash after peer disconnect" "$O" "ERROR"
close_conn 3

# ─── SECTION 16: Flood / stopped client ──────────────────────────────────────
header "Flood / Stopped Client"
open_conn 3 fop; open_conn 4 fvic
send_fd 3 "JOIN #flood\r\n"
send_fd 4 "JOIN #flood\r\n"; sleep 0.2
# fvic is "stopped" — we don't read from fd 4
for i in $(seq 1 50); do printf "PRIVMSG #flood :msg $i\r\n" >&3; done
sleep 0.5
OUT=$(nc_cmd "PASS pass\r\nNICK fcheck\r\nUSER fchecku 0 * :fcheck\r\n")
check "Server not hanging after 50-msg flood to stopped client" "$OUT" "001 fcheck"
close_conn 3; close_conn 4

# ─── SECTION 17: MODE query & edge cases ─────────────────────────────────────
header "Edge Cases"
open_conn 3 eop
send_fd 3 "JOIN #eroom\r\n"
send_fd 3 "MODE #eroom +t\r\nMODE #eroom +i\r\nMODE #eroom +l 5\r\n"; sleep 0.3
send_fd 3 "MODE #eroom\r\n"; sleep 0.3
O=$(read_fd 3)
check "MODE query (no flags) → 324 with current modes" "$O" "324"
check "324 includes +t" "$O" "t"
check "324 includes +i" "$O" "i"

# NICK change after registration
send_fd 3 "NICK enewnick\r\n"; sleep 0.3
O2=$(read_fd 3)
check "NICK change post-registration works" "$O2" "eop.*NICK :enewnick"
check_not "NICK change → no 432/433 error" "$O2" "43[23]"

# JOIN without # (silently ignored)
send_fd 3 "JOIN nochannel\r\n"; sleep 0.2
O3=$(read_fd 3)
check_not "JOIN without # → no ERROR crash" "$O3" "ERROR"

# TOPIC with no args
send_fd 3 "TOPIC\r\n"; sleep 0.3
O4=$(read_fd 3)
check "TOPIC with no channel → 461" "$O4" "461 TOPIC"

# PRIVMSG with no args
send_fd 3 "PRIVMSG\r\n"; sleep 0.2
O5=$(read_fd 3)
check_not "PRIVMSG with no args → no crash" "$O5" "ERROR"

# KICK with only nick (no channel)
send_fd 3 "KICK eop :test\r\n"; sleep 0.2
O6=$(read_fd 3)
check_not "KICK with only nick (missing channel) → no crash" "$O6" "ERROR"
close_conn 3

# ─── SECTION 18: Multiple operator commands in sequence ──────────────────────
header "Operator Commands — Full Sequence"
open_conn 3 boss; open_conn 4 sub1; open_conn 5 sub2
send_fd 3 "JOIN #boss\r\n"
send_fd 4 "JOIN #boss\r\n"
send_fd 5 "JOIN #boss\r\n"
sleep 0.3

# Set topic
send_fd 3 "TOPIC #boss :boss channel\r\n"; sleep 0.3
S=$(read_fd 4)
check "TOPIC broadcast to all members" "$S" "boss channel"

# Give sub1 operator
send_fd 3 "MODE #boss +o sub1\r\n"; sleep 0.3

# sub1 changes topic (both ops can now)
send_fd 4 "TOPIC #boss :sub1 topic\r\n"; sleep 0.3
O=$(read_fd 3)
check "New op can set TOPIC" "$O" "sub1 topic"

# sub1 kicks sub2
send_fd 4 "KICK #boss sub2 :out\r\n"; sleep 0.4
V=$(read_fd 5)
check "New op can KICK" "$V" "KICK #boss sub2"

# sub2 tries to rejoin (not invite-only, should work)
send_fd 5 "JOIN #boss\r\n"; sleep 0.3
V2=$(read_fd 5)
check "Kicked user can rejoin normal channel" "$V2" "sub2.*JOIN #boss"

# Set +k then sub2 rejoins with key
send_fd 3 "MODE #boss +k secret\r\n"; sleep 0.3
send_fd 4 "KICK #boss sub2 :again\r\n"; sleep 0.3
send_fd 5 "JOIN #boss\r\n"; sleep 0.3
V3=$(read_fd 5)
check "Rejoining +k channel without key → 475" "$V3" "475"
send_fd 5 "JOIN #boss secret\r\n"; sleep 0.3
V4=$(read_fd 5)
check "Rejoining +k channel with correct key → success" "$V4" "sub2.*JOIN #boss"
close_conn 3; close_conn 4; close_conn 5

# ─── RESULTS ─────────────────────────────────────────────────────────────────
echo ""
echo "═══════════════════════════════════════"
total=$((pass+fail))
echo -e "  ${GREEN}PASSED: $pass / $total${NC}"
[ $fail -gt 0 ] && echo -e "  ${RED}FAILED: $fail / $total${NC}"
echo "═══════════════════════════════════════"
[ $fail -eq 0 ] && echo -e "  ${GREEN}ALL TESTS PASSED${NC}"

kill $SRV_PID 2>/dev/null
