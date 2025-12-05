# Projeto Final Redes Sem Fio - Sistema de Alarme Residencial Controlado via Wi-Fi

Por: Bruno Binelli e Bruno Carboni


## 1. Descrição do Projeto Final:

O projeto final consiste no desenvolvimento de um sistema de alarme residencial conectado. O sistema será controlado por uma interface web hospedada diretamente em um dispositivo embarcado (ESP32), que atuará como um ponto de acesso Wi-Fi. Isso permitirá que o usuário (por exemplo, através de um smartphone) se conecte à rede do dispositivo e habilite ou desabilite o sistema de alarme.

O sistema utilizará um conjunto de sensores e atuadores para simular um ambiente de segurança:

* Um sensor ultrassônico (HC-SR04) detectará a presença na frente de uma porta.
* Um buzzer soará como alarme sonoro caso o sensor detecte presença e o alarme esteja habilitado.
* Um módulo relé será usado para simular porta abrindo ou fechando, fornecendo feedback audível da operação.

Além disso, o projeto implementará um sistema de log. Um segundo ESP32 atuará como um "banco" de dados, recebendo e armazenando todos os eventos do sistema (ex: "alarme habilitado", "alarme disparado") com data e hora. Esta implementação visa explorar a comunicação sem fio entre dois dispositivos embarcados.

### Aplicação Desenvolvida:

A aplicação está centralizada no **ESP32-Servidor**. Este dispositivo criará uma rede Wi-Fi (modo SoftAP). O usuário conecta seu celular a esta rede, abre um navegador e acessa o IP do ESP32 (ex: 192.168.4.1) para carregar a interface web de controle.

A interface permitirá:

* **Habilitar/Desabilitar o Alarme:** Um botão na página web controlará o estado do sistema.
* **Acionar o Relé:** Botões permitirão ao usuário "abrir" ou "fechar" a porta (simulada pelo relé).

Quando o alarme estiver habilitado, o ESP32-Servidor monitorará continuamente o sensor HC-SR04. Se uma presença for detectada (distância < 30 cm), o buzzer será ativado.

Paralelamente, o **ESP32-Servidor** enviará todos os eventos (logs) para o **ESP32-Log** através de uma segunda comunicação sem fio. O ESP32-Log receberá esses dados, adicionará um timestamp (data/hora) e os armazenará.

### Componentes de Hardware:

A seguir, detalhamos os componentes de hardware que serão utilizados no projeto.

* **ESP32 (2 unidades):** Microcontrolador com Wi-Fi e Bluetooth integrados.
    * **ESP32-Servidor:** Gerencia todos os periféricos (HC-SR04, Relé, Buzzer) e atua como Ponto de Acesso (AP) Wi-Fi para hospedar a interface web.
    * **ESP32-Log:** Atua como um "banco" de dados, recebendo e armazenando logs.
* **Sensor Ultrassônico HC-SR04:** Utilizado para detecção de presença. Mede a distância por pulsos de sonar.
* **Módulo Relé (SRD-05VDC-SL-C):** Atuador eletromecânico. Será usado para simular a porta.
* **Buzzer Passivo:** Atuador sonoro. Servirá como alarme principal.

<div align="center">
  <img src="img-circ.JPG" alt="Esquema de Ligação" width="400">
</div>

### 1.3 Componentes de Software:

A seguir, detalhamos os componentes de software que serão desenvolvidos e aproveitados.

* **Código Desenvolvido (ESP32-Servidor):**
    * Lógica de controle principal.
    * Lógica de leitura.
    * Servidor web HTTP para receber requisições GET (ex: `/ligarrele`, `/habilitaralarme`).
    * Página web (HTML/CSS) embarcada no código C++ como uma string.
    * Protocolo de comunicação (cliente) para enviar logs ao ESP32-Log.

* **Código Desenvolvido (ESP32-Log):**
    * Protocolo de comunicação (servidor) para receber logs.
    * Lógica de armazenamento.

* **Bibliotecas de Terceiros (Aproveitadas):**
    * `WiFi.h`: Para a funcionalidade de Ponto de Acesso (SoftAP) e comunicação Wi-Fi geral.
    * `WebServer.h`: Para a criação do servidor HTTP no ESP32-Servidor.
    * `ESPNow.h`: Uma alternativa leve ao Wi-Fi tradicional para a comunicação ESP-a-ESP.

## 2. Análise de Desafios Técnicos e Soluções Implementadas

Durante o desenvolvimento do sistema de alarme distribuído, foram identificadas limitações inerentes à arquitetura de *hardware* (ESP32) e ao protocolo de comunicação escolhido (ESP-NOW). Abaixo, discutimos os principais desafios enfrentados e as soluções de engenharia adotadas para contorná-los, bem como uma análise crítica das premissas assumidas.

### 2.1. Limitação de Payload no Protocolo ESP-NOW

**O Desafio:**
O protocolo ESP-NOW, embora eficiente energeticamente e de baixa latência, impõe um limite rígido (*hard limit*) de **250 bytes** por pacote de dados. Essa restrição tornou-se um obstáculo para a transmissão de logs detalhados ou históricos acumulados, que excediam facilmente esse tamanho.

**Solução Adotada:**
Implementou-se uma lógica de **Fragmentação na Camada de Aplicação**. O *firmware* do Controlador fatia mensagens longas em segmentos de **190 bytes** e os envia sequencialmente ao Logger.

**Análise Crítica:**
Embora a solução permita o envio de mensagens longas, o ESP-NOW opera de forma similar ao protocolo UDP (User Datagram Protocol), sem garantia nativa de ordem de entrega.
* **Limitação:** Em ambientes com alta interferência eletromagnética, existe a possibilidade de o "Segmento 2" chegar antes do "Segmento 1", ou de perda de pacotes sem solicitação automática de reenvio.
* **Perspectiva Industrial:** Em uma implementação comercial robusta, seria necessário implementar um cabeçalho customizado contendo ID do pacote e número de sequência (ex: `MsgID: 10, Seq: 1/3`), permitindo ao receptor remontar a mensagem ordenadamente ou solicitar retransmissão (ARQ). Para o escopo deste projeto, a fragmentação simples ("fire and forget") foi considerada uma premissa aceitável.

### 2.2. Ausência de Referencial de Tempo Real (RTC)

**O Desafio:**
O microcontrolador ESP32 não possui, nativamente, um relógio de tempo real persistente. Como o sistema opera em uma rede isolada (modo *SoftAP*) sem acesso à Internet, não é possível utilizar o protocolo NTP (*Network Time Protocol*) para sincronização. Além disso, o projeto não incluiu um módulo RTC externo (como o DS3231).

**Solução Adotada:**
Optou-se pela utilização do **"Uptime"** (tempo de atividade do sistema) como referencial temporal para os logs.

**Análise Crítica:**
Esta abordagem assume que o usuário tem conhecimento do momento exato em que o sistema foi ligado.
* **Limitação:** Se o sistema reiniciar (por falha de energia ou *watchdog*), o contador temporal é zerado, perdendo-se a correlação cronológica com eventos passados. Embora funcional para monitoramento imediato, esta solução inviabiliza o sistema para fins de auditoria forense rigorosa, onde data e hora absolutas são mandatórias.

### 2.3. Persistência de Dados: Memória RAM vs. Flash

**O Desafio:**
A definição do local de armazenamento dos logs gerou um *trade-off* entre persistência e durabilidade do *hardware*.

**Solução Adotada:**
Os logs são armazenados em uma variável do tipo `String` alocada na **memória RAM** (Volátil).

**Análise Crítica:**
Considerou-se a utilização do sistema de arquivos interno (SPIFFS ou LittleFS) para salvar os logs em memória Flash não-volátil (arquivos `.txt`). No entanto, essa opção foi descartada devido à natureza do sensor de presença.
* **Justificativa Técnica:** Memórias Flash possuem um limite finito de ciclos de escrita (tipicamente entre 10.000 e 100.000 ciclos). Como o sensor pode gerar eventos em alta frequência, a gravação contínua na Flash causaria o desgaste prematuro (*wear-out*) do chip.
* **Conclusão:** A RAM suporta ciclos de escrita infinitos, sendo a escolha tecnicamente correta para *buffers* de alta rotatividade. O ponto de falha (perda de dados ao desligar) seria mitigado em um produto final através da inclusão de uma bateria de *backup* para o microcontrolador.

## 3. Resultados:

O projeto realizado pode ser considerado um protótipo funcional e integrado do sistema de alarme. O sistema operou conforme as especificações, permitindo o controle via interface web e a atuação correta dos sensores e atuadores.

Além da implementação do produto funcional, o trabalho permitiu a análise prática das limitações e desafios inerentes às redes sem fio utilizadas, observando-se os seguintes pontos:

* **Responsividade:** O servidor web hospedado no ESP32 demonstrou desempenho satisfatório para a aplicação proposta. A interface de controle apresentou tempo de resposta adequado para o acionamento do relé e habilitação do alarme, validando a capacidade do microcontrolador em gerenciar requisições HTTP simultaneamente ao monitoramento dos sensores.
* **Interferência:** Foi possível analisar a estabilidade das conexões do projeto em ambientes reais. Observou-se o comportamento da rede SoftAP frente à presença de outras redes Wi-Fi e dispositivos Bluetooth, permitindo identificar a robustez do link estabelecido entre o smartphone e o ESP32-Servidor.
* **Desafios da Comunicação ESP com ESP:** A implementação do sistema de logs entre os dois ESP32s foi concluída com êxito. O trabalho consolidou a discussão sobre a escolha do protocolo de comunicação, avaliando na prática a eficácia da solução adotada em termos de consumo de energia, complexidade de implementação e a garantia de entrega dos pacotes de dados (logs) com seus respectivos timestamps.

## 4. Setup do Sistema de Alarme Residencial IoT (ESP32):

Este guia descreve o fluxo completo para configurar o hardware e o software do sistema de alarme conectado.

---

### 🛠️ Fase 1: Montagem do Hardware

Você precisará de **dois** microcontroladores ESP32:
1.  **ESP32-Controlador:** Conectado aos sensores e atuadores.
2.  **ESP32-Log:** Atua como servidor de logs (apenas alimentação USB).

#### 1. Conexões do ESP32-Controlador

Conecte os periféricos conforme a tabela abaixo:

| Componente | Pino do Componente | Pino do ESP32 | Observação |
| :--- | :--- | :--- | :--- |
| **Sensor HC-SR04** | VCC | 5V (VIN) | Alimentação |
| | GND | GND | Terra |
| | TRIG | **GPIO 26** | Disparo do ultrassom |
| | ECHO | **GPIO 25** | Retorno do sinal* |
| **Módulo Relé** | VCC | 3.3V ou 5V | Verificar especificação do módulo |
| | GND | GND | Terra |
| | IN (Sinal) | **GPIO 27** | Controle da porta |
| **Buzzer** | Positivo (+) | **GPIO 14** | Sinal sonoro |
| | Negativo (-) | GND | Terra |

> **⚠️ Atenção (Proteção de Hardware):** O pino ECHO do HC-SR04 opera logicamente em 5V, enquanto o ESP32 opera em 3.3V. Recomenda-se o uso de um **divisor de tensão** (Resistores de 1kΩ e 2kΩ) entre o ECHO e o GPIO 25 para proteger a porta do microcontrolador.

#### 2. Conexões do ESP32-Log
Este dispositivo não requer conexões externas de GPIO. Apenas conecte-o via USB para alimentação e monitoramento via Serial.

---

### 💻 Fase 2: Setup do Software (Arduino IDE)

1.  **Instalar Arduino IDE:** Baixe a versão 2.x ou superior no [site oficial](https://www.arduino.cc/en/software).
2.  **Configurar Placa ESP32:**
    * Vá em `File > Preferences`.
    * Em "Additional Boards Manager URLs", adicione:
        `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
    * Vá em `Tools > Board > Boards Manager`, procure por "esp32" (por Espressif Systems) e instale.
3.  **Dependências:**
    * O projeto utiliza apenas bibliotecas nativas do Core ESP32 (`WiFi.h`, `esp_now.h`, `WebServer.h`, `DNSServer.h`). Nenhuma instalação adicional é necessária.

---

### 🚀 Fase 3: Upload dos Códigos

Identifique fisicamente qual placa será o **Controlador** e qual será o **Log**.

#### Passo A: Gravando o ESP32-Controlador
1.  Conecte o ESP32 principal ao PC.
2.  Selecione a porta correta em `Tools > Port`.
3.  Abra o arquivo `esp_controller.ino`.
4.  Clique em **Upload** (Seta para direita).
    * *Nota:* Este código cria o AP `Alarm_System` (Senha: `12345678`).

#### Passo B: Gravando o ESP32-Log
1.  Conecte o segundo ESP32 (Logger).
2.  Selecione a nova porta COM em `Tools > Port`.
3.  Abra o arquivo `esp_log.ino`.
4.  Clique em **Upload**.
5.  Após gravar, abra o **Serial Monitor** (Baud Rate: 115200) para confirmar se a mensagem `Logger Device Ready` aparece.

---

### 📱 Fase 4: Operação e Teste

1.  **Alimentação:** Ligue ambos os ESP32.
2.  **Conexão Wi-Fi:**
    * No seu smartphone ou PC, procure a rede Wi-Fi: **`Alarm_System`**.
    * Senha: **`12345678`**.
3.  **Acesso à Interface:**
    * Abra o navegador e digite o IP: **`192.168.4.1`**.
    * A interface de controle (Dashboard) será carregada:

<div align="center">
    <img src="tela.png" alt="Esquema de Ligação" width="400">
</div>
    
4.  **Teste de Funcionalidade:**
    * Clique em **ARM ALARM**.
    * Acione o sensor ultrassônico (movimento < 30cm). O Buzzer deve soar.
    * Clique em **VIEW HISTORY**. O sistema irá recuperar os logs do segundo ESP32 e exibi-los na tela com timestamp.
