*This project has been created as part of the 42 curriculum by aaleixo-, fragarc2 and mde-maga.*

# ft_irc

## Description

ft_irc is a fully functional IRC server written in C++98. It handles multiple clients simultaneously using a single `poll()` call for all I/O operations (non-blocking), and communicates over TCP/IP.

Clients connect using any standard IRC client. The server supports authentication, channels, private messages, and all required channel operator commands.

## Features

- Non-blocking I/O with a single `poll()` loop
- Password-protected server access
- Nickname and username registration
- Multiple concurrent clients
- Channel creation and management
- Partial command buffering (handles split TCP packets)

**Supported commands:** `PASS`, `NICK`, `USER`, `JOIN`, `PRIVMSG`, `KICK`, `INVITE`, `TOPIC`, `MODE`, `PART`, `QUIT`, `PING`, `HELP`

**Channel modes:** `+i` (invite-only), `+t` (topic restricted), `+k` (key/password), `+o` (operator), `+l` (user limit)

## Instructions

### Compile

```bash
make
```

### Run

```bash
./ircserv <port> <password>
```

### Example

```bash
./ircserv 6667 mypassword
```

### Connect with nc (for testing)

```bash
nc -C 127.0.0.1 6667
```

### Connect with an IRC client

Any standard IRC client works (e.g. irssi, WeeChat, LimeChat). Set the server to `127.0.0.1`, the port to whatever you chose, and the password to the one you passed at startup.

---

## Connection Flow

Every client must follow this sequence to register and start chatting:

```
Step 1 — Authenticate
  PASS <server_password>

Step 2 — Set your nickname
  NICK yournick

Step 3 — Set your username
  USER yourusername 0 * :Your Real Name

  ✓ You are now registered and auto-joined to #general

Step 4 — Join a channel
  JOIN #mychannel

Step 5 — Send a message
  PRIVMSG #mychannel :Hello everyone!

Step 6 — Message a specific user
  PRIVMSG othernick :Hey, what's up?
```

Type `HELP` at any point to see all available commands.

---

## Command Reference

### Basic commands

| Command | Usage | Description |
|---------|-------|-------------|
| `PASS` | `PASS <password>` | Server password — must be sent first |
| `NICK` | `NICK <nickname>` | Set or change your nickname |
| `USER` | `USER <user> 0 * :<name>` | Set your username |
| `JOIN` | `JOIN #channel [key]` | Join or create a channel |
| `PART` | `PART #channel [:reason]` | Leave a channel |
| `PRIVMSG` | `PRIVMSG <target> :<message>` | Send a message to a channel or user |
| `TOPIC` | `TOPIC #channel [:new topic]` | View or set the channel topic |
| `QUIT` | `QUIT [:reason]` | Disconnect from the server |
| `HELP` | `HELP` | Show all available commands |

### Operator-only commands

The first user to create a channel becomes its operator. Operators can manage the channel using these commands:

| Command | Usage | Description |
|---------|-------|-------------|
| `KICK` | `KICK #channel <nick> [:reason]` | Remove a user from the channel |
| `INVITE` | `INVITE <nick> #channel` | Invite a user (required when channel is +i) |
| `MODE` | `MODE #channel <+/-flags> [args]` | Change channel modes (see below) |

### Channel modes

| Mode | Set / Unset | Description |
|------|-------------|-------------|
| `+i` / `-i` | `MODE #channel +i` | Invite-only — only invited users can join |
| `+t` / `-t` | `MODE #channel +t` | Only operators can change the topic |
| `+k` / `-k` | `MODE #channel +k <key>` | Require a password to join |
| `+o` / `-o` | `MODE #channel +o <nick>` | Give or take operator status |
| `+l` / `-l` | `MODE #channel +l <number>` | Limit max number of users in channel |

---

## Resources

**IRC specifications**
- [RFC 1459](https://datatracker.ietf.org/doc/html/rfc1459) — Internet Relay Chat Protocol
- [RFC 2812](https://datatracker.ietf.org/doc/html/rfc2812) — IRC Client Protocol

**Networking**
- Beej's Guide to Network Programming
- Linux man pages: `poll(2)`, `socket(2)`, `bind(2)`, `listen(2)`, `accept(2)`, `recv(2)`, `send(2)`, `fcntl(2)`

