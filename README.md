# zmk-config — teste do joystick

Firmware ZMK mínimo pra testar **só o joystick analógico** como mouse, no
ProMicro nRF52840 (clone nice!nano). Board target: `nice_nano_v2`.

## Fiação

| Sinal | Pino board | nRF52840 | ADC |
|---|---|---|---|
| Joystick X | D19 | P0.02 | AIN0 |
| Joystick Y | D20 | P0.29 | AIN5 |
| VCC joystick | **3.3V** | — | — |
| GND joystick | GND | — | — |

⚠️ Alimente o joystick em **3.3V**, nunca 5V (o ADC satura acima de ~3.6V).

## Como gerar o firmware (GitHub Actions)

1. Crie um repositório no GitHub e suba **o conteúdo desta pasta** (`zmk-config/`)
   como raiz do repo:
   ```bash
   cd zmk-config
   git init && git add . && git commit -m "joystick test config"
   git branch -M main
   git remote add origin git@github.com:SEU_USUARIO/zmk-config.git
   git push -u origin main
   ```
2. Vá na aba **Actions** do repo. O build roda sozinho no push.
3. Ao terminar, baixe o artefato **`firmware`** → dentro tem `jstest nice_nano_v2.uf2`.

## Como gravar (flash)

1. Conecte o board via USB.
2. Dê **reset duplo** (dois toques rápidos no botão) → aparece um drive USB
   chamado `NICENANO` (ou similar).
3. Arraste o `.uf2` pra dentro. O board reinicia sozinho já rodando o firmware.

## O que esperar

- Ao mexer o joystick, o **cursor do mouse** se move (via USB HID).
- Parado no centro, o cursor não deve andar (zona morta).

## Ajuste fino (arquivo `config/boards/shields/jstest/jstest.overlay`)

- **Cursor anda sozinho parado** → aumente `mv-deadzone`, ou corrija `mv-mid`
  pro valor real do centro (em mV).
- **Cursor rápido/lento demais** → mude `scale-divisor` (maior = mais lento).
- **Eixo Y (ou X) invertido** → descomente/comente `invert;` naquele eixo.
- **Não move num eixo** → confira fiação daquele pino e o `NRF_SAADC_AINx`.

Pra ver os valores brutos do ADC e calibrar `mv-mid`, descomente
`CONFIG_ZMK_USB_LOGGING=y` em `config/jstest.conf` e abra o monitor serial.

## ⚠️ Nota sobre bateria (uso sem fio)

Este driver lê o joystick em **polling contínuo** (100 Hz) → consumo alto,
**não ideal pra sem fio**. Pro teste via USB é irrelevante. Pro firmware final
sem fio, vale reavaliar (ler o joystick sob demanda ou aceitar o tradeoff).
