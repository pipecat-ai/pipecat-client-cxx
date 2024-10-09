# Daily Bots custom server example

Daily Bots requires a developer hosted endpoint which can host custom API keys
for different services (OpenAI, etc.), the Daily Bots profile that should be
used or anything that should be hidden in the client.

This is a super simple example that expects a list of desired Daily Bots
services and a configuration to be used by the bot.

## Install project dependencies

```shell
npm install
```

## Create your local environment

```shell
cp env.example .env.local
```

Enter your API keys (and add any service specific private keys here if needed,
e.g. OpenAI).

#### Start the dev server

```shell
node server.js
```

This will make http://localhost:3000 available which will be your custom Daily
Bots base URL.
