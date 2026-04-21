# ft_irc — Documentação do Projeto

> Servidor IRC escrito em C++98 com um único loop `poll()`. Clientes conectam via TCP, autenticam com senha, registram apelido/usuário e podem entrar em canais, trocar mensagens e usar comandos de operador. Sem threads
**Como rodar:** `./ircserv <port> <password>`  
**Como conectar:** irssi, WeeChat, ou `nc localhost <port>`

---

## Fluxo Geral

```text
main.cpp
  │  Valida porta (1-65535) e senha
  │  Cria Server(porta, senha)
  └─► Server::serverer()  ← loop principal, nunca faz return

Server::serverer()
  │  Cria socket TCP, faz bind, começa a escutar
  │  Adiciona socket do servidor na lista do poll()
  │  Loop:
  │    poll() — bloqueia até algo acontecer
  │    Se fd do servidor disparar → accept() novo cliente
  │    Se fd de cliente disparar  → recv() bytes, chama clientRead()
  │    Se POLLHUP/POLLERR         → desconecta cliente
  └─► Repete para sempre

Client::clientRead(fd, buffer, bytes)
  │  Acumula bytes no buffer do cliente (lida com pacotes TCP parciais)
  │  Divide em linhas por \r\n ou \n
  │  Cliente NÃO registrado → Client::Register()
  └─► Cliente registrado    → Client::sendMessage()

Client::Register(fd, linha)   — trata PASS / NICK / USER
  │  Exige: senha primeiro, depois nick, depois user
  │  Ao completar: entra automaticamente em #general
  └─► Envia MOTD de boas-vindas

Client::sendMessage(fd, linha)   — despacha todos os comandos normais
  Analisa a palavra de comando e chama o handler certo:
    JOIN, PART, PRIVMSG, TOPIC, MODE, KICK, INVITE, NICK, QUIT, PING, HELP

Métodos de Channel cuidam de:
  addMember, removeMember, kick, invite, setTopic, mode, broadcast
```

---

## Server — `Server`

### O que faz

Possui o socket de escuta e a lista de file descriptors do `poll()`. Não sabe nada sobre comandos IRC — só move bytes brutos para a camada Client e encerra conexões mortas.

### Membros principais

| Membro | Tipo | Descrição |
|---|---|---|
| `_serverSocket` | `int` | Socket TCP de escuta |
| `_fds` | `vector<pollfd>` | Um por conexão aberta + servidor |
| `_port` | `int` | Porta configurada |
| `_password` | `string` | Senha do servidor |
| `_cli` | `Client` | Objeto que possui todo o estado de clientes/canais |

### Fluxo detalhado

**1. Configuração do socket**

```bash
socket(AF_INET, SOCK_STREAM, 0)
setsockopt SO_REUSEADDR   # permite reusar a porta rapidamente após restart
fcntl O_NONBLOCK          # não-bloqueante para o poll() conseguir multiplexar
bind() + listen()
```

**2. Loop do poll()**

- **fd do servidor dispara** → `accept()` nova conexão, seta não-bloqueante, adiciona ao `_fds`, cria entrada no mapa de clientes
- **fd de cliente dispara** → `recv()` até 1024 bytes. Se `bytes <= 0`: desconecta. Senão: `_cli.clientRead(fd, buf, bytes)`
- **POLLHUP/POLLERR** → desconecta

**3. Limpeza ao desconectar**

```bash
_cli.removeCli(fd)   # remove de todos os canais + mapa de clientes
close(fd)
erase do vetor _fds
```

> **Por que `poll()` e não `select()`?** `poll()` não tem limite de tamanho no fd_set e é mais limpo para listas dinâmicas.

---

## Client — `Client`

### O que faz

Possui todo o estado por conexão (`t_client`) e todos os objetos de canal. Cuida de autenticação, parsing de comandos e roteamento de mensagens.

### Estruturas de dados principais

**`t_client` — struct com uma instância por socket conectado**

| Campo | Tipo | Descrição |
|---|---|---|
| `fd` | `int` | File descriptor do socket |
| `nick` | `string` | Apelido escolhido (máx. 9 chars) |
| `user` | `string` | Username (do comando `USER`) |
| `pass` | `string` | Senha fornecida (do comando `PASS`) |
| `buf` | `string` | Buffer de acumulação de mensagens parciais |
| `registered` | `bool` | `true` após `PASS` + `NICK` + `USER` completos |

**Mapas da classe `Client`**

| Membro | Tipo | Descrição |
|---|---|---|
| `_clients` | `map<int, t_client>` | fd → dados do cliente |
| `_channels` | `map<string, Channel>` | nome do canal → objeto Channel |
| `_serverPassword` | `string` | Senha de referência para checar `PASS` |

### Fluxo de registro — `Client::Register`

O cliente deve enviar nesta ordem:

1. `PASS <password>` — validada contra `_serverPassword`; rejeitada se errada
2. `NICK <nickname>` — validado por `isValidNick()`; rejeitado se em uso
3. `USER <user> ...` — username extraído

**Quando os três estão definidos:**

- `registered = true`
- Cliente entra automaticamente em `#general`
- Primeiro cliente em `#general` vira operador
- `sendWelcome()` envia sequência de boas-vindas (001/002/003/004) + MOTD

**Erros durante o registro:**

| Código | Motivo |
|---|---|
| `464` | Senha errada |
| `431` | Nenhum apelido fornecido |
| `432` | Formato de apelido inválido |
| `433` | Apelido já em uso |
| `451` | Tentou enviar comandos antes de se registrar |

### Despacho de comandos — `Client::sendMessage`

#### `JOIN #channel [key]`

- Cria o canal se não existir (criador vira operador)
- Chama `channel->addMember()` — checa invite-only, key, limite de usuários
- Se entrou com sucesso: broadcast `":nick!user@localhost JOIN #channel"` para todos
- Envia tópico existente (332) ao cliente que entrou, se houver

#### `PRIVMSG <target> :message`

- Target começa com `#` → `channel->broadcast()` para todos exceto o remetente
- Target é um nickname → envia diretamente para aquele fd
- Target desconhecido → erro 401/403

#### `TOPIC #channel [:newtopic]`

- Sem argumento → responde com tópico atual (332) ou "sem tópico" (331)
- Com argumento → `channel->setTopic()` (respeita modo `+t`)
- Ao mudar → broadcast da mensagem TOPIC para todos no canal

#### `MODE #channel <flags> [args]`

Analisa prefixo `+`/`-` e cada letra de flag:

| Flag | Efeito |
|---|---|
| `+i` / `-i` | Invite-only ligado/desligado |
| `+t` / `-t` | Tópico restrito a operadores |
| `+k` / `-k` | Key do canal definida/removida |
| `+o` / `-o` | Dar/tirar operador (precisa de nick alvo) |
| `+l` / `-l` | Limite de usuários definido/removido (precisa de número) |

Todas as mudanças de MODE exigem que o remetente seja operador.

#### `KICK #channel <nick> [reason]`

- Remetente deve ser operador
- Chama `channel->kick()` → broadcast da mensagem KICK, remove o alvo

#### `INVITE <nick> #channel`

- Remetente deve ser operador
- Chama `channel->invite()` → adiciona à lista de convites
- Envia notificação INVITE para o nick alvo

#### `NICK <newnick>`

- Valida formato (`isValidNick`) e unicidade
- Atualiza o nick no `t_client`
- Faz broadcast da mudança de NICK para todos os canais do usuário

#### `PART #channel [reason]`

- Broadcast da mensagem PART para o canal
- Chama `channel->removeMember()`
- Se o canal ficou vazio → deleta o canal

#### `QUIT [reason]`

- Broadcast do QUIT para todos os canais do usuário
- Chama `removeCli()` → limpa todas as participações

#### `PING`

- Responde com PONG (mantém conexão viva)

#### `CAP`

- Reconhecido silenciosamente (compatibilidade com clientes IRC)

#### `HELP`

- Envia lista de comandos disponíveis ao cliente

### Funções auxiliares (estáticas no arquivo)

**`isValidNick(nick)`**
- Máx. 9 chars
- Primeiro char: letra ou `_`
- Demais chars: alfanumérico ou `-_[]\^{}|`

**`isNickInUse(cli, nick, excludeFd)`**
- Percorre mapa `_clients`; faz return `true` se nick bater com qualquer fd exceto `excludeFd`

**`findClient(cli, nick)` → `int fd`**
- Busca linear em `_clients` pelo apelido; faz return `-1` se não encontrado

**`findChannelByName(cli, name)` → `Channel*`**
- Busca no mapa `_channels`; faz return de ponteiro ou `NULL`

---

## Channel — `Channel.*`

### O que faz

Representa um único canal IRC. Possui lista de membros, operadores, lista de convites, tópico e flags de modo. Sabe como aplicar suas próprias regras.

### Membros principais

| Membro | Tipo | Descrição |
|---|---|---|
| `name` | `string` | Nome do canal (ex: `#general`) |
| `topic` | `string` | Tópico atual |
| `members` | `vector<t_client*>` | Todos os clientes no canal |
| `operators` | `vector<t_client*>` | Subconjunto com privilégio de op |
| `inviteList` | `vector<t_client*>` | Autorizados a burlar `+i` |
| `inviteOnly` | `bool` | Modo `+i` |
| `topicRestricted` | `bool` | Modo `+t` |
| `key` | `string` | Modo `+k` (vazio se sem senha) |
| `userLimit` | `size_t` | Modo `+l` (0 = sem limite) |

### Gerenciamento de membros

**`addMember(Client*, t_client*, providedKey)`**

Checa nesta ordem:

1. Já é membro? → return silencioso
2. `+i` e não convidado? → erro 473, return
3. `+k` e chave errada? → erro 475, return
4. `+l` e canal cheio? → erro 471, return
5. Passou tudo → adiciona ao vetor `members`

**`removeMember(t_client*)`**

- Remove do vetor `members`
- Remove também do vetor `operators` (status de op perdido ao sair)

**`isMember(t_client*)` / `isOperator(t_client*)`**

- Busca linear nos vetores correspondentes

### Operações de canal

**`setTopic(newTopic, Client*, sender)`**

- Se modo `+t` ativo e sender não é op → erro 482, return
- Caso contrário atualiza a string do tópico

**`kick(Client*, sender, target)`**

- Broadcast `":sender!user@localhost KICK #channel target :reason"` para todos
- Chama `removeMember(target)`

**`invite(Client*, sender, target)`**

- Checa se já está convidado (evita duplicatas)
- Adiciona à `inviteList`
- Envia confirmação 341 ao sender

**`mode(sender, flag, value, target, param)`**

- `'i'` → `inviteOnly = (value == '+')`
- `'t'` → `topicRestricted = (value == '+')`
- `'k'` → `key = (value == '+') ? param : ""`
- `'l'` → `userLimit = (value == '+') ? atoi(param) : 0`
- `'o'` → `addOperator` / `removeOperator` no target
- Faz broadcast da mensagem MODE para todos os membros

**`broadcast(Client*, msg, excludeFd = -1)`**

- Percorre vetor `members`
- Envia `msg` para todo membro cujo fd != `excludeFd`
- Usado por: JOIN, PART, PRIVMSG, TOPIC, KICK, MODE, QUIT

---

## Correções de Bugs Recentes

> Commit `74eea6d` — último no branch `dev`

### 1. Formato errado nos códigos de erro IRC

**Antes:** erros eram enviados como texto simples sem prefixo do servidor:

```text
473 nick #channel :Cannot join channel (+i)
```

**Depois:** seguem o formato correto com prefixo IRC:

```text
:ft_irc 473 nick #channel :Cannot join channel (+i)
```

**Por quê importa:** clientes IRC precisam do prefixo `:ft_irc` para saber quem enviou o erro. Sem ele, erros não eram exibidos corretamente.

Afetados: `473`, `475`, `471`, `482`, `401`, `403`, `331`, `332`, `341`, `461`.

---

### 2. JOIN não fazia broadcast para membros existentes

**Antes:** quando alguém entrava num canal, só o cliente que entrou recebia a confirmação de JOIN.  
**Depois:** `channel->broadcast()` é chamado, então todos os membros veem a notificação — como no IRC de verdade.

---

### 3. JOIN não enviava o tópico existente

**Antes:** ao entrar num canal com tópico definido, o cliente não recebia nenhuma informação sobre o tópico.  
**Depois:** após JOIN bem-sucedido, se existir tópico o servidor envia `332 nick #channel :topic` imediatamente.

---

### 4. JOIN ignorava a key do canal

**Antes:** `JOIN #channel mypassword` — a key era lida mas nunca passada para `addMember`, então canais com `+k` podiam ser entrados por qualquer um.  
**Depois:** a key é extraída da linha JOIN e encaminhada para `channel->addMember()` onde é validada.

---

### 5. Criador do canal não virava operador

**Antes:** quem criava um canal não recebia status de operador automaticamente, deixando o canal sem ninguém capaz de usar KICK/INVITE/MODE.  
**Depois:** após criar o canal (`isNew == true`), o criador é imediatamente promovido via `channel->addOperator()`.

---

### 6. KICK não checava status de operador

**Antes:** qualquer membro podia kickar outros.  
**Depois:** handler de KICK checa `isOperator()` antes de prosseguir; envia erro `482` se não for op.

---

### 7. INVITE não estava implementado de verdade

**Antes:** handler do INVITE mal existia — sem checagem de op, sem erros, sem notificação ao convidado.  
**Depois:** implementação completa — checa op, erros `403`/`401`/`482`, chama `channel->invite()`, envia notificação INVITE ao alvo.

---

### 8. PRIVMSG só funcionava para canais, não mensagens diretas

**Antes:** PRIVMSG sempre procurava um canal; enviar para um apelido não fazia nada.  
**Depois:** se o target não começa com `#`, `findClient()` resolve o apelido para um fd e envia diretamente; erro `401` se não encontrado.

---

### 9. Função `broadcast()` não existia

**Antes:** todo lugar que precisava enviar para membros do canal duplicava um loop manual sobre `_clients` — iterando TODOS os clientes do servidor, não só os do canal.  
**Depois:** `Channel::broadcast(Client*, msg, excludeFd)` é um método próprio que itera apenas o vetor `members` do canal.

---

### 10. Validação de apelido estava faltando

**Antes:** qualquer string era aceita como apelido.  
**Depois:** `isValidNick()` aplica as regras IRC: máx. 9 chars, primeiro char deve ser letra ou `_`, demais chars alfanuméricos ou `-_[]\^{}|`. `isNickInUse()` previne apelidos duplicados.

---

### 11. Accessor `getMembers()` adicionado

**Antes:** não havia como iterar membros de um canal de fora da classe Channel.  
**Depois:** `getMembers()` faz return de referência const, usado pelo QUIT para fazer broadcast para todos os canais do usuário que saiu.

---

## Exemplos de Fluxo de Dados

### Conectar e se registrar

```text
Cliente conecta
  → Server aceita, cria t_client{fd, "", "", "", false, ""}

Cliente envia:  PASS mypassword
  → Register() valida contra _serverPassword, armazena em t_client.pass

Cliente envia:  NICK alice
  → Register() checa isValidNick("alice") ok, isNickInUse() ok, armazena nick

Cliente envia:  USER alice 0 * :Alice Smith
  → Register() armazena user, seta registered=true
  → Entra em #general automaticamente, alice vira operadora
  → sendWelcome() envia linhas 001/002/003/004 + MOTD
```

### Enviar mensagem num canal

```text
alice digita:  PRIVMSG #general :oi gente

sendMessage() analisa: cmd="PRIVMSG", target="#general", msg="oi gente"
  → findChannelByName() encontra objeto Channel de #general
  → channel->broadcast(cli, ":alice!alice@localhost PRIVMSG #general :oi gente", alice_fd)
  → Todos os membros exceto alice recebem a mensagem
```

### Kickar um usuário

```text
alice (op) digita:  KICK #general bob :tchau

sendMessage() analisa: cmd="KICK", chanName="#general", target="bob"
  → isOperator(&alice_t_client) → true
  → findClient(cli, "bob") → bob_fd
  → channel->kick(cli, &alice_t_client, &bob_t_client)
      faz broadcast de ":alice!alice@localhost KICK #general bob :tchau" para todos
      chama removeMember(&bob_t_client)
```

### Definir modo no canal

```text
alice (op) digita:  MODE #general +k senhasecreta

sendMessage() analisa: cmd="MODE", chanName="#general", flag="+k", param="senhasecreta"
  → isOperator(&alice) → true
  → channel->mode(&alice, '+', 'k', NULL, "senhasecreta")
      key = "senhasecreta"
      broadcast de ":alice!alice@localhost MODE #general +k senhasecreta"
```

---

## Estrutura de Arquivos

```text
ft_irc/
├── main.cpp                  Ponto de entrada, validação de args
├── Makefile                  Compila binário 'ircserv' (C++98, -Wall -Wextra -Werror)
├── README.md                 Referência de comandos e uso
├── DOCUMENTACAO.md           Este arquivo
├── test_thorough.sh          Suite de testes de integração (200+ testes via netcat)
├── network/
│   ├── Server.hpp            Definição da classe Server
│   └── Server.cpp            Loop poll(), setup de socket, accept/recv/cleanup
├── client/
│   ├── Client.hpp            Struct t_client, definição da classe Client
│   └── Client.cpp            Registro, despacho de comandos, todos os handlers IRC
└── channel/
    ├── Channel.hpp           Definição da classe Channel
    └── Channel.cpp           Gerenciamento de membros, modos, kick/invite/broadcast
```

---

## Códigos Numéricos IRC

| Código | Significado |
|---|---|
| `001`–`004` | Sequência de boas-vindas ao se registrar |
| `331` | Nenhum tópico definido |
| `332` | Resposta com tópico atual |
| `341` | Confirmação de convite |
| `401` | Nick/canal não encontrado |
| `403` | Canal não encontrado |
| `431` | Nenhum apelido fornecido |
| `432` | Apelido inválido |
| `433` | Apelido já em uso |
| `451` | Não registrado |
| `461` | Parâmetros insuficientes |
| `464` | Senha incorreta |
| `471` | Canal cheio (`+l`) |
| `473` | Canal invite-only (`+i`) |
| `475` | Key do canal errada (`+k`) |
| `482` | Não é operador do canal |
