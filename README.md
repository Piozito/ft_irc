*This project has been created as part of the 42 curriculum by aaleixo-, fragarc2 and mde-maga.*

# ft_irc

## Description
ft_irc is a custom Internet Relay Chat (IRC) server written in C++.  
The goal of the project is to implement a functional IRC server that handles
multiple clients concurrently, supports basic IRC commands, and follows the
RFC specifications where applicable.

## Instructions

### Run
```bash
./ircserv <port> <password>
```

### Example
```bash
./ircserv 4242 pass
```

## Resources
**Classic references**
- RFC 1459 — Internet Relay Chat Protocol
- RFC 2812 — Internet Relay Chat: Client Protocol
- Beej’s Guide to Network Programming
- Linux `poll`, `socket`, `bind`, `listen`, `accept`, `recv`, `send` man pages

**AI usage**
- ChatGPT (OpenAI) was used to draft and format this README and to clarify
  documentation requirements. No production code was generated or modified
  by AI.

## Notes
This README provides a high-level overview and basic usage. Additional
features or project-specific requirements can be added as needed.