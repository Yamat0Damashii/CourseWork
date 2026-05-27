# DHT Messenger Coursework

Консольный мессенджер для коротких сообщений поверх DHT-хранилища.

## Выбранная DHT

В качестве основы выбрана Kademlia-подобная DHT, потому что именно на ней построена популярная BitTorrent Mainline DHT. В реальных сетях такая таблица используется для поиска значений по ключу без центрального сервера: узлы знают часть соседей, принимают команды поиска и хранения, а значение находится по хешу ключа.

В проекте реализована небольшая учебная DHT-библиотека `MiniDhtNode`:

- UDP-транспорт между узлами.
- Идентификатор узла как 64-битный FNV-1a хеш имени.
- Bootstrap через параметр `--peer host:port`.
- Команды `PING`, `PONG`, `STORE`, `FIND`, `VALUE`.
- Локальное хранилище `key -> values`.
- Распространение маленьких значений по известным peer-узлам.

Это не промышленная реализация полного Kademlia routing table с k-buckets, но модель соответствует курсовой задаче: прикладной слой работает не напрямую через TCP-сессию, а через распределенное key-value DHT с маленьким размером значения.

## Протокол сообщений

DHT плохо подходит для передачи больших сообщений одним значением, поэтому мессенджер использует собственный протокол фрагментации.

Формат фрагмента:

```text
DMSG|1|message_id|fragment_index|fragment_total|payload_size|fragment_crc32|message_crc32|payload_hex
```

Поля:

- `DMSG` - сигнатура пакета.
- `1` - версия протокола.
- `message_id` - случайный 128-битный идентификатор сообщения в hex.
- `fragment_index` - номер фрагмента с нуля.
- `fragment_total` - общее количество фрагментов.
- `payload_size` - размер полезной нагрузки до hex-кодирования.
- `fragment_crc32` - контроль целостности конкретного фрагмента.
- `message_crc32` - контроль целостности всего собранного сообщения.
- `payload_hex` - данные фрагмента в hex.

Ключи DHT:

```text
inbox:<hash(alias)>       -> список message_id для получателя
msg:<message_id>:<index>  -> конкретный фрагмент сообщения
```

Получатель сначала читает свой `inbox`, затем по каждому `message_id` запрашивает фрагменты `msg:*`, проверяет CRC каждого фрагмента, сверяет заголовки и в конце проверяет CRC всего сообщения.

## Запуск

Собрать в Visual Studio 2022 или через MSBuild:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' .\CourseWork.slnx /p:Configuration=Debug /p:Platform=x64 /m
```

Запустить два узла в разных терминалах:

```powershell
.\x64\Debug\CourseWork.exe --name alice --port 7001
.\x64\Debug\CourseWork.exe --name bob --port 7002 --peer 127.0.0.1:7001
```

Отправить сообщение из Bob в Alice:

```text
send alice Привет через DHT
```

Проверить входящие у Alice:

```text
poll
```

Команды приложения:

```text
send <recipient> <text>  send a short message through the DHT
poll                     check inbox once
listen                   poll inbox until Ctrl+C
peers                    show DHT status
help                     show commands
quit                     exit
```

## Файлы

- `CourseWork/main.cpp` - реализация UDP DHT, протокола фрагментов и консольного интерфейса.
- `CourseWork/CourseWork.vcxproj` - проект Visual Studio, C++20, линковка с `ws2_32.lib`.
