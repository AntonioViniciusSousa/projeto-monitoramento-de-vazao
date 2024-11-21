/*
  Projeto: Sistema de monitoramento de vazão
  Curso: Técnico em Informática para Internet
  Desenvolvedor: Antônio Vinícius da Silva Sousa
  Orientador: Heraldo Antunes Silva Filho
  Data: 10/05/2024
*/

// Bibliotecas do projeto.
#include <dht.h>                     // Biblioteca do sensor de umidade e temperatura
#include <Wire.h>                    // Biblioteca para comunicação I2C
#include <LiquidCrystal_I2C.h>       // Biblioteca para controle de display LCD I2C
#include <SD.h>                      // Biblioteca para leitura e escrita no cartão SD
#include <SPI.h>
#include <ThreeWire.h>               // Biblioteca para comunicação com RTC DS1302
#include <RtcDS1302.h>               // Biblioteca específica para RTC DS1302

// Definições e Criação de Objetos.
#define PINO_TRIGGER 7               // Pino associado ao pino de disparo (Trig) do sensor ultrassônico
#define PINO_ECHO 8                  // Pino associado ao pino de eco (Echo) do sensor ultrassônico
#define PINO_DHT11 6                 // Pino associado ao sinal do sensor de umidade e temperatura
#define N 1                          // Número de medições para a média móvel centrada
dht sensorDHT11;                     // Inicialização do sensor de umidade e temperatura

long duracao;                        // Definição da variável para contagem da quantidade de pulsos
float velocidade_do_som;             // Definição da variável para a velocidade do som
float distancia;                     // Utilizada p/ calcular a distância do sensor (em centímetros)
float distancia_em_metros;           // Utilizada p/ calcular a distância do sensor (em metros)
float v[N];                          // Vetor para média móvel centrada das distâncias
float media_das_distancias;          // Recebe a média das distâncias
float altura = 0.0;                  // Altura detectada pelo sensor (nível da água em metros)
float altura_real;                   // Altura real detectada pelo sensor (nível da água em centímetros)
float vazao;                         // Variável que recebe os dados de vazão
float soma = 0.0;                    // Recebe a soma das distâncias para calcular a média
float umidade;                       // Variável que recebe a umidade
float temperatura;                   // Variável que recebe a temperatura
float vazao_pela_equacao;            // Variável que armazena o valor da vazão de acordo com a equação dos testes efetuados
float vazao_pelo_grafico;            // Variável que armazena o valor da vazão de acordo com a equação do gráfico
float vazao_pelos_testes;
float vazao_calha;

const int INTERRUPCAO_SENSOR = 0;    // Definição do pino de interrupção do sensor de vazão (Interrupt = 0 equivale ao pino digital 2)
const int PINO_SENSOR = 2;           // Definição do pino do sensor de vazão
unsigned long contador = 0;          // Definição da variável de contagem de giros
const float FATOR_CALIBRACAO = 4.5;  // Definição do fator de calibração para conversão do valor lido
const float FATOR_CALIBRACAO1 = 6.6; // Definição do fator de calibração para conversão do valor lido
const float FATOR_CALIBRACAO2 = 7.5; // Definição do fator de calibração para conversão do valor lido
unsigned long tempo_antes = 0;       // Definição da variável de intervalo de tempo
float fluxo = 0;                     // Definição da variável de fluxo
float fluxo1 = 0;                    // Definição da variável de fluxo
float fluxo2 = 0;                    // Definição da variável de fluxo
float fluxo_em_segundo = 0;          // Definição da variável de fluxo (em segundo)
float volume_total = 0;              // Definição da variável de volume total

// Definições e Criação de Objetos
#define chipSelect 10 // Pino de chip select (CS) do módulo SD

// Definições do módulo RTC DS1302
#define DS1302_RST  3 // Pino RST do RTC conectado ao pino digital 11 do Arduino
#define DS1302_DAT  4 // Pino DAT do RTC conectado ao pino digital 13 do Arduino
#define DS1302_CLK  5 // Pino CLK do RTC conectado ao pino digital 2 do Arduino
ThreeWire rtcWire(DS1302_DAT, DS1302_CLK, DS1302_RST); // Inicialização dos pinos do RTC
RtcDS1302<ThreeWire> rtc(rtcWire); // Inicialização do RTC DS1302
bool cartao_habilitado = true; // Estado inicial do cartão SD (habilitado)

// Instanciando Objetos.
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  Serial.begin(9600);                  // Inicialização do Monitor Serial
  Wire.begin();                        // Inicializa a comunicação I2C
  lcd.init();                          // Inicia a comunicação com o Display LCD
  lcd.backlight();                     // Liga a iluminação do Display LCD
  lcd.clear();                         // Limpa o Display LCD
  pinMode(PINO_SENSOR, INPUT_PULLUP);  // Configuração do pino do sensor ultrassônico como entrada em nível lógico alto
  pinMode(PINO_TRIGGER, OUTPUT);       // Configuração do pino do sensor de umidade e temperatura como saída
  pinMode(PINO_ECHO, INPUT);           // Configuração do pino do sensor de umidade e temperatura como entrada
  sensorDHT11.read11(PINO_DHT11);      // Inicialização do sensor de umidade e temperatura
  
  // Inicialização do SD
  if (!SD.begin(chipSelect)) {
    Serial.println("Falha ao inicializar o SD");
    lcd.setCursor(0, 0);
    lcd.print("Erro no SD");
    while (1);
  }

  // Inicialização do RTC DS1302
  rtc.Begin(); // Inicializa o RTC
}

void loop() {
  sensorDHT11.read11(PINO_DHT11);          // Inicialização do sensor de umidade e temperatura
  umidade = sensorDHT11.humidity;         // Associação do sensor de umidade
  temperatura = sensorDHT11.temperature;  // Associação do sensor de temperatura

  // Calcula as distâncias armezenadas em um vetor (evitar ruídos na medida do sensor).
  for (int i = 0; i < N; i++) {
    digitalWrite(PINO_TRIGGER, LOW);     // Desabilita o trigpin do sensor setando como LOW
    delayMicroseconds(5);                // HIGH por 5 microsegundos
    digitalWrite(PINO_TRIGGER, HIGH);    // Aciona o Trigger do sensor setando o trigpin
    delayMicroseconds(10);               // HIGH por 10 microsegundos
    digitalWrite(PINO_TRIGGER, LOW);     // Desabilita o trigpin do sensor setando como LOW
    duracao = pulseIn(PINO_ECHO, HIGH);  // Leitura do echopin e retorna a duração (Comprimentodo do Pulso) em microsegundos

    // Calcula a velocidade do som, corrigindo com os fatores de temperatura e umidade.
    velocidade_do_som = 331.3 + (0.606 * temperatura + 0.0124 * umidade);

    // Calcula a distância em centímetros.
    distancia = (duracao * (velocidade_do_som / 10000) / 2);
    distancia_em_metros = distancia / 100;  // Calcula a distância em metros
    v[i] = distancia_em_metros;             // Armazena as distâncias em um vetor
    delay(1);                               // Delay de 1 microsegundo
    soma = soma + v[i];                     // Soma as distâncias em um vetor
  }

  media_das_distancias = soma / N;  // Média móvel centrada para filtragem do sinal do sensor
  altura = media_das_distancias;
  altura_real = (17.25 - distancia);

  // Executa a contagem de pulsos uma vez por segundo.
  if ((millis() - tempo_antes) > 1000) {
    detachInterrupt(INTERRUPCAO_SENSOR);                                          // Desabilita a interrupção para realizar a conversão do valor de pulsos
    fluxo = ((1000.0 / (millis() - tempo_antes)) * contador) / FATOR_CALIBRACAO;  // Conversão do valor de pulsos para L/min
    fluxo1 = ((1000.0 / (millis() - tempo_antes)) * contador) / FATOR_CALIBRACAO1;       // Conversão do valor de pulsos para L/min
    fluxo2 = ((1000.0 / (millis() - tempo_antes)) * contador) / FATOR_CALIBRACAO2;       // Conversão do valor de pulsos para L/min

    fluxo_em_segundo = fluxo / 60;     // Conversão do valor de pulsos de L/min para L/s
    volume_total += fluxo_em_segundo;  // Armazenamento do volume

    vazao_pela_equacao = (0.0014 * contador) + 6.0581;
    vazao_pelo_grafico = (2.3467 * contador) / 7.6529;
    vazao_pelos_testes = (contador - 0.0002) + 3.1071;

    
    vazao_calha = (1.9419 * altura_real) + 1.3586;
    

    //Exibição dos dados no Monitor Serial.
    Serial.print("Pulsos (giros): ");
    Serial.println(contador);
    Serial.print("Fluxo(L/min): ");
    Serial.println(fluxo, 4);
    Serial.print("Fluxo_6.6(L/min): ");
    Serial.println(fluxo1, 4);
    Serial.print("Fluixo_7.5(L/min): ");
    Serial.println(fluxo2, 4);
    Serial.print("Vazão pela equação(L/min): ");
    Serial.println(vazao_pela_equacao, 4);
    Serial.print("Vazão pelo gráfico(L/min): ");
    Serial.println(vazao_pelo_grafico, 4);
    Serial.print("Vazão pelos testes(L/min): ");
    Serial.println(vazao_pelos_testes, 4);

    contador = 0;                                                  // Reinicialização do contador de pulsos
    tempo_antes = millis();                                        // Atualização da variável tempo_antes
    attachInterrupt(INTERRUPCAO_SENSOR, contador_pulso, FALLING);  // Contagem de pulsos do sensor
  }

  //Exibição dos dados no Monitor Serial.
  Serial.print("Umidade(%): ");
  Serial.println(umidade, 4);
  Serial.print("Temperatura(ºC): ");
  Serial.println(temperatura, 4);
  Serial.print("Velocidade do som(m/s): ");
  Serial.println(velocidade_do_som, 4);
  Serial.print("Distância(cm): ");
  Serial.println(distancia, 4);
  Serial.print("Distância(m): ");
  Serial.println(distancia_em_metros, 4);
  Serial.print("Distância real(cm): ");
  Serial.println(altura_real, 4);
  Serial.print("Volume(L): ");
  Serial.println(volume_total, 4);
  Serial.println("- - - - - - - - - -");

  //Exibição dos dados no Display LCD.
  lcd.setCursor(0, 0);  // Posiciona o cursor na primeira coluna da linha 1
  lcd.print("MEDIDOR DE VAZAO");
  
  lcd.setCursor(0, 1);  // Posiciona o cursor na primeira coluna da linha 1
  lcd.print("NIVEL(cm):");
  lcd.setCursor(10, 1);
  lcd.print(altura_real, 2);

  lcd.setCursor(0, 2);  // Posiciona o cursor na primeira coluna da linha 1
  lcd.print("VAZAO(L/min):");
  lcd.setCursor(13, 2);
  lcd.print(vazao_calha, 2);

  lcd.setCursor(0,3);  // Posiciona o cursor na primeira coluna da linha 1
  lcd.print("VOLUME(L):");
  lcd.setCursor(10, 3);
  lcd.print(volume_total, 2);
  
  delay(1000);
  lcd.clear();

  RtcDateTime now = rtc.GetDateTime(); // Lê a data e hora atuais do RTC

  if (cartao_habilitado) {
      contador_dados++; // Incrementa o contador de dados
      File dataFile = SD.open("datalog.txt", FILE_WRITE); // Abre o arquivo datalog.txt para escrita
      if (dataFile) {
        dataFile.print("Dado ");
        dataFile.print(contador_dados);
        dataFile.print(": ");
        dataFile.print(now.Day()); // Escreve o dia no arquivo
        dataFile.print("/"); // Escreve "/" no arquivo
        dataFile.print(now.Month()); // Escreve o mês no arquivo
        dataFile.print("/"); // Escreve "/" no arquivo
        dataFile.print(String(now.Year() - 2000)); // Escreve o ano com dois dígitos no arquivo
        dataFile.print(" "); // Escreve um espaço no arquivo
        dataFile.print(now.Hour()); // Escreve a hora no arquivo
        dataFile.print(":"); // Escreve ":" no arquivo
        dataFile.print(now.Minute()); // Escreve os minutos no arquivo
        dataFile.print(":"); // Escreve ":" no arquivo
        dataFile.print(now.Second()); // Escreve os segundos no arquivo
        dataFile.print(", Temperatura(°C) = "); // Escreve ", " no arquivo
        dataFile.print(temperatura, 2); // Escreve a média de Temperatura (em Celsius) no arquivo
        dataFile.print(", Umidade(%) = "); // Escreve ", " no arquivo
        dataFile.print(umidade, 2); // Escreve a média de Umidade no arquivo
        dataFile.print(", Nivel(cm) = "); // Escreve ", " no arquivo
        dataFile.print(altura_real, 2); // Escreve a média de Temperatura (em Fahrenheit) no arquivo
        dataFile.print(", Vazao pelo sensor ultrassonico(L/min) = "); // Escreve ", " no arquivo
        dataFile.print(vazao_calha, 2); // Escreve a média do Índice de Calor (em Fahrenheit) no arquivo
        dataFile.print(", Vazao pelo sensor de fluxo(L/min) = "); // Escreve ", " no arquivo
        dataFile.print(fluxo1, 2); // Escreve a média do Índice de Calor (em Fahrenheit) no arquivo
        dataFile.println(); // Escreve uma nova linha no arquivo
        dataFile.close(); // Fecha o arquivo

        Serial.print("Dado salvo: ");
        Serial.print("Temperatura: ");
        Serial.print(temperatura, 2);
        Serial.print(" C, Umidade: ");
        Serial.print(umidade, 2);
        Serial.print(", Nivel(cm) = ");
        Serial.print(altura_real, 2); // Escreve a média de Temperatura (em Fahrenheit) no arquivo
        Serial.print(", Vazao pelo sensor ultrassonico(L/min) = "); // Escreve ", " no arquivo
        Serial.print(vazao_calha, 2); // Escreve a média do Índice de Calor (em Fahrenheit) no arquivo
        Serial.print(", Vazao pelo sensor de fluxo(L/min) = "); // Escreve ", " no arquivo
        Serial.print(fluxo1, 2); // Escreve a média do Índice de Calor (em Celsius) no arquivo
        Serial.println(); // Escreve uma nova linha no arquivo
        
      } else {
        Serial.println("Erro ao abrir o arquivo datalog.txt"); // Mensagem de erro ao abrir o arquivo
        //digitalWrite(LED_ERRO, HIGH); // Liga o LED de erro
      }
  }
  

  // Reinicialização das Variáveis.
  altura = 0;  // Reinicialização do contador da altura
  soma = 0;    // Reinicialização do contador da soma das distâncias
}

// Função chamada pela interrupção para contagem de pulsos.
void contador_pulso() {
  contador++;
}
