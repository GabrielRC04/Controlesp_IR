#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Tcl.h>

// Rede em que a ESP vai se conectar
const char* ssid_casa = "NomedoWifi";
const char* senha_casa = "SenhadoWifi";

// Rede que a ESP vai transmitir
const char* ssid = "ESP8266";
const char* password = "12345678";

// Configurações do NTP
const char* ntpServer = "a.st1.ntp.br"; 
const long  gmtOffset_sec = -3 * 3600;  
const int   daylightOffset_sec = 0;     

// Configurações hardware
const uint16_t kIrLed = 5; // D1 no ESP8266 = GPIO5
IRTcl112Ac ac(kIrLed);
// Comando para ligar o ar-condicionado na temperatura de 24 graus
uint8_t ligar[14] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x13, 0x07, 0x38, 0x00, 0x00, 0x00, 0x00, 0x8B};

int temperatura = 25; 
bool ledState = false; 

ESP8266WebServer server(80);

// --- ESTRUTURA DO AGENDAMENTO ---
struct Agenda {
  bool ativa;
  bool isSemanal; // false = Ação Simples, true = Rotina Semanal
  int hora;
  int minuto;
  bool acaoLigar; // true = Ligar, false = Desligar
  bool dias[7];   // 0=Dom, 1=Seg, 2=Ter, 3=Qua, 4=Qui, 5=Sex, 6=Sab (Usado apenas se isSemanal for true)
  bool executadoHoje; // Evita que a rotina dispare várias vezes no mesmo minuto
};

#define MAX_AGENDAS 10
Agenda agendas[MAX_AGENDAS];
int ultimoMinuto = -1;

// Funções base de hardware (separadas para podermos chamar via Web ou via Timer)
void ligarLED() {
  digitalWrite(LED_BUILTIN, LOW);
  ledState = true;
  ac.setRaw(ligar);
  //Serial.println(ac.toString());
  ac.send();
  Serial.println("Comando enviado!");
}

void desligarLED() {
  digitalWrite(LED_BUILTIN, HIGH);
  ledState = false;
}

// --- GERAÇÃO DA INTERFACE HTML ---
String getHTML() {
  String html;
  html.reserve(4000); // Previne fragmentação de memória
  html += "<!DOCTYPE html><html lang='pt-BR'>";
  html += "<head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Controle ESP8266</title>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;background:#f2f2f2;margin-top:20px;}";
  html += "h1,h2{color:#333;}";
  html += ".status{font-size:18px;margin:10px;}";
  html += ".on{color:green;font-weight:bold;} .off{color:red;font-weight:bold;}";
  html += "button{padding:10px 20px;font-size:16px;margin:5px;border:none;border-radius:8px;cursor:pointer;}";
  html += ".btn-on{background:#4CAF50;color:white;} .btn-off{background:#f44336;color:white;}";
  html += ".btn-temp{background:#2196F3;color:white;} .btn-del{background:#999;color:white;padding:5px 10px;font-size:12px;}";
  html += "input, select{padding:8px;font-size:16px;border-radius:5px;border:1px solid #ccc;margin:5px;}";
  html += ".section{background:white;padding:15px;border-radius:10px;display:inline-block;margin:10px;box-shadow:0px 0px 10px rgba(0,0,0,0.1); width: 320px; vertical-align:top;}";
  html += ".agenda-item{background:#f9f9f9;border:1px solid #ddd;padding:10px;margin-bottom:10px;border-radius:5px;text-align:left;font-size:14px;}";
  html += "</style>";
  
  // Script para mostrar/esconder os dias da semana dependendo do tipo de agenda
  html += "<script>";
  html += "function toggleDias() {";
  html += "  var tipo = document.getElementById('tipo').value;";
  html += "  document.getElementById('divDias').style.display = (tipo == '1') ? 'block' : 'none';";
  html += "}";
  html += "</script>";
  html += "</head><body>";
  
  html += "<h1>Painel ESP8266</h1>";

  // SEÇÃO DO LED
  html += "<div class='section'><h2>Controle Manual</h2>";
  html += "<div class='status'>Status: " + String(ledState ? "<span class='on'>LIGADO</span>" : "<span class='off'>DESLIGADO</span>") + "</div>";
  html += "<a href='/ligar'><button class='btn-on'>Ligar</button></a>";
  html += "<a href='/desligar'><button class='btn-off'>Desligar</button></a>";
  html += "</div>";

  // SEÇÃO DA TEMPERATURA
  html += "<div class='section'><h2>Ajuste de Temperatura</h2>";
  html += "<div class='status'>Atual: <strong>" + String(temperatura) + " &deg;C</strong></div>";
  html += "<form action='/temperatura' method='GET'>";
  html += "<input type='number' name='valor' value='" + String(temperatura) + "' min='0' max='100' style='width:70px;'>";
  html += "<button type='submit' class='btn-temp'>Definir</button></form>";
  html += "</div>";

  // SEÇÃO DE AGENDAMENTOS (CRIAR)
  html += "<div class='section'><h2>Nova Agenda</h2>";
  html += "<form action='/addAgenda' method='GET'>";
  html += "<select name='tipo' id='tipo' onchange='toggleDias()'>";
  html += "<option value='0'>Ação Simples (1 vez)</option>";
  html += "<option value='1'>Rotina Semanal</option></select><br>";
  
  html += "<input type='time' name='hora' required><br>";
  
  html += "<select name='acao'>";
  html += "<option value='1'>Ligar LED</option>";
  html += "<option value='0'>Desligar LED</option></select><br>";
  
  html += "<div id='divDias' style='display:none; text-align:left; margin-left:30px;'>";
  html += "<label><input type='checkbox' name='d0'> Dom</label> <label><input type='checkbox' name='d1'> Seg</label><br>";
  html += "<label><input type='checkbox' name='d2'> Ter</label> <label><input type='checkbox' name='d3'> Qua</label><br>";
  html += "<label><input type='checkbox' name='d4'> Qui</label> <label><input type='checkbox' name='d5'> Sex</label><br>";
  html += "<label><input type='checkbox' name='d6'> Sab</label>";
  html += "</div>";
  html += "<button type='submit' class='btn-temp' style='width:100%;'>Salvar Agenda</button>";
  html += "</form></div>";

  // SEÇÃO DE AGENDAMENTOS (LISTAR)
  html += "<div class='section'><h2>Agendas Ativas</h2>";
  bool temAgenda = false;
  for (int i = 0; i < MAX_AGENDAS; i++) {
    if (agendas[i].ativa) {
      temAgenda = true;
      html += "<div class='agenda-item'>";
      char horaFormat[6];
      sprintf(horaFormat, "%02d:%02d", agendas[i].hora, agendas[i].minuto);
      html += "<strong>" + String(horaFormat) + "</strong> - " + (agendas[i].acaoLigar ? "<span class='on'>Ligar</span>" : "<span class='off'>Desligar</span>") + "<br>";
      
      if (agendas[i].isSemanal) {
        html += "<small>Dias: ";
        const char* nomesDias[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
        for(int d=0; d<7; d++) if(agendas[i].dias[d]) html += String(nomesDias[d]) + " ";
        html += "</small>";
      } else {
        html += "<small>Ação Simples (Executa 1x)</small>";
      }
      html += "<br><a href='/delAgenda?id=" + String(i) + "'><button class='btn-del'>Remover</button></a>";
      html += "</div>";
    }
  }
  if (!temAgenda) html += "<p>Nenhuma agenda configurada.</p>";
  html += "</div>";

  html += "</body></html>";
  return html;
}

// --- HANDLERS HTTP ---
void handleRoot() { server.send(200, "text/html", getHTML()); }

void handleLigar() {
  ligarLED();
  server.sendHeader("Location", "/"); // Redireciona para evitar URL suja
  server.send(303);
}

void handleDesligar() {
  desligarLED();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handletemperatura() {
  if (server.hasArg("valor")) temperatura = server.arg("valor").toInt(); 
  server.sendHeader("Location", "/");
  server.send(303); 
}

void handleAddAgenda() {
  // Procura espaço vazio no array
  int idLivre = -1;
  for(int i=0; i<MAX_AGENDAS; i++) {
    if(!agendas[i].ativa) { idLivre = i; break; }
  }

  if(idLivre != -1 && server.hasArg("hora") && server.hasArg("tipo") && server.hasArg("acao")) {
    String horaStr = server.arg("hora"); // Formato "HH:MM"
    agendas[idLivre].hora = horaStr.substring(0, 2).toInt();
    agendas[idLivre].minuto = horaStr.substring(3, 5).toInt();
    agendas[idLivre].isSemanal = (server.arg("tipo") == "1");
    agendas[idLivre].acaoLigar = (server.arg("acao") == "1");
    
    agendas[idLivre].dias[0] = server.hasArg("d0");
    agendas[idLivre].dias[1] = server.hasArg("d1");
    agendas[idLivre].dias[2] = server.hasArg("d2");
    agendas[idLivre].dias[3] = server.hasArg("d3");
    agendas[idLivre].dias[4] = server.hasArg("d4");
    agendas[idLivre].dias[5] = server.hasArg("d5");
    agendas[idLivre].dias[6] = server.hasArg("d6");
    
    agendas[idLivre].executadoHoje = false;
    agendas[idLivre].ativa = true;
  }
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDelAgenda() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    if(id >= 0 && id < MAX_AGENDAS) {
      agendas[id].ativa = false;
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  ac.begin();
  // Inicializa o array de agendas
  for(int i=0; i<MAX_AGENDAS; i++) agendas[i].ativa = false;

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid_casa, senha_casa);
  WiFi.softAP(ssid, password);

  Serial.println("\nModo Dual ativado!");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");

  Serial.print("IP na rede local (Casa): ");
  Serial.println(WiFi.localIP());

  Serial.print("IP do Ponto de Acesso (ESP): ");
  Serial.println(WiFi.softAPIP());
  
  Serial.println("Sincronizando NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  while (time(nullptr) < 100000) { delay(500); Serial.print("."); }
  Serial.println("\nHora sincronizada!");

  pinMode(LED_BUILTIN, OUTPUT);
  desligarLED(); // Inicia desligado

  // Removido o "WiFi.mode(WIFI_AP);" que estava aqui, pois ele desconectava a ESP do roteador e parava o NTP.

  server.on("/", handleRoot);
  server.on("/ligar", handleLigar);
  server.on("/desligar", handleDesligar);
  server.on("/temperatura", handletemperatura);
  server.on("/addAgenda", handleAddAgenda);
  server.on("/delAgenda", handleDelAgenda);

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();

  // --- LÓGICA DO AGENDAMENTO ---
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  // Reseta a flag 'executadoHoje' caso o minuto vire
  if (timeinfo->tm_min != ultimoMinuto) {
    ultimoMinuto = timeinfo->tm_min;
    for (int i = 0; i < MAX_AGENDAS; i++) {
      agendas[i].executadoHoje = false;
    }
  }

  // Verifica as rotinas ativas
  for (int i = 0; i < MAX_AGENDAS; i++) {
    if (agendas[i].ativa && !agendas[i].executadoHoje) {
      if (agendas[i].hora == timeinfo->tm_hour && agendas[i].minuto == timeinfo->tm_min) {
        
        bool deveExecutar = false;
        
        if (agendas[i].isSemanal) {
          // Checa se o dia atual da semana (0=Dom...6=Sab) está marcado
          if (agendas[i].dias[timeinfo->tm_wday]) {
            deveExecutar = true;
          }
        } else {
          // Ação simples
          deveExecutar = true;
        }

        if (deveExecutar) {
          if (agendas[i].acaoLigar) ligarLED();
          else desligarLED();
          
          agendas[i].executadoHoje = true;
          
          // Se for ação simples (executa só uma vez), desativa após executar
          if (!agendas[i].isSemanal) {
            agendas[i].ativa = false;
          }
        }
      }
    }
  }
}
