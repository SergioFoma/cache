# Многоуровневая система кеширования ⚡

This is C++ Vladimirov task.
Collaborator SergioFoma.

## Лабораторная работа

[Отчёт по исследованию иерархии кешей](report/REPORT.md): восемь видов нагрузки,
сравнение алгоритмов и ёмкостей, двухуровневые конфигурации и пропорции их ёмкости.
Датасеты, результаты и средства воспроизведения находятся в `report/`.

![C++](https://img.shields.io/badge/C++-20-blue?logo=cplusplus)
![CMake](https://img.shields.io/badge/CMake-3.11-064F8C?logo=cmake)
![GoogleTest](https://img.shields.io/badge/GoogleTest-passing-4285F4?logo=googletest&logoColor=white)

## Структура проекта 📝

```
├──📂cache
│    ├──📄CMakeLists.txt                  # CMake
│    ├──📄.clang-format                   # Форматирование кода (Google C++ Style)
|    ├──📄.clang-tidy                     # Исправление стиля
│    ├──📄README.md                       # README
│    ├──📂include                         # Заголовочные файлы
│    │    ├──📂cache_algorithm            # Алгоритмы кеширования
|    │    │    ├──📄arc.hpp               # ARC (adaptive replacement cache)
|    │    │    ├──📄belady_cache.hpp      # Идеальный кеш
|    │    │    ├──📄lfu_cache.hpp         # LFU (вытеснение наименее часто используемого)
|    │    │    ├──📄lirs_cache.hpp        # LIRS (low-inference recency set)
|    │    │    ├──📄lru_cache.hpp         # LRU
|    │    │    ├──📄two_queues.hpp        # 2Q (алгоритм двух очередей, дальнейшее развитие LRU)
│    │    ├──📄cache_hierarchy.hpp        # Иерархия кеширования
│    │    ├──📄cache.hpp                  # Абстрактный класс кеша
│    │    └──📄parser.hpp                 # Обработка входных данных
│    ├──📂src                             # Исходные файлы
│    │    ├──📂tests                      # Тесты
|    │    │    ├──📄cache_algorithm.cpp   # Unit-tests
|    │    │    ├──📄gen_test.cpp          # Cache misses test generator
│    │    ├──📄main.cpp                   # Main
|    │    └──📄parser.cpp                 # Обработка входных данных
│    ├──📂tests                           # Сгенерированные данные для сравнения cache misses
│    ├──📂config
│    │    ├──📄config.txt                 # Задание иерархии кешей в текстовом виде
|    │    └──📄config.json                # Задание иерархии кешей через json формат


```

## Скачивание репозитория 👾

### **Чтобы скачать весь проект, нужно:**

1. В Вашем терминале необходимо перейти в директорию, куда Вы хотите скачать данный проект.

2. Затем в терминале введите:

SSH:

```bash
git clone git@github.com:pr1usf0x/cache.git
```

HTTPS:

```bash
git clone https://github.com/pr1usf0x/cache.git
```

3. Дождитесь скачивания.

4. После чего будет создана рабочая директория с названием репозитория (cache).

## Запуск программы 🚀

Собрать проект можно последовательным выполнением команд:

```bash
cmake -S . -B Build
cmake --build Build
```

**Работа с программой:**

- выводит в stdout help программы:
```bash
Build/cache --help
```

- начинает чтение входных данных из stdin: размер кеша, размер данных и далее данные:
```bash
Build/cache
```
- пример:
```bash
2 4 1 2 1 2
```

- запускает Google Tests (83 теста)
```bash
Build/cache_algorithm_test
```

## Collaborators 👤

[SergioFoma](https://github.com/SergioFoma)
