# Spike: first-class collections jako expression (`UserIds::{1, 2, 3}`)

**Pytanie:** czy składnia `UserIds::{1, 2, 3}` — literał kolekcji tworzony przez
alias/nazwę klasy — jest możliwa **wszędzie jako expression**?

**Werdykt:** w formie `::{ ... }` — **NIE**. Kolizja z istniejącą funkcją języka.
W formie `::[ ... ]` — **TAK**, delimiter jest wolny. Szczegóły niżej.

---

## 1. Dlaczego `ClassName::{ ... }` odpada

Ta produkcja **już istnieje** w gramatyce — `Zend/zend_language_parser.y`,
reguła `class_constant` (linie 1531–1534):

```
class_name          T_PAAMAYIM_NEKUDOTAYIM '{' expr '}'   { ZEND_AST_CLASS_CONST }
variable_class_name T_PAAMAYIM_NEKUDOTAYIM '{' expr '}'   { ZEND_AST_CLASS_CONST }
```

To **dynamiczny dostęp do stałej klasowej** (PHP 8.3+). Znaczenie dzisiaj:

| Zapis            | Znaczenie dziś                                   |
|------------------|--------------------------------------------------|
| `Foo::{$name}`   | stała klasy `Foo` o nazwie z `$name`             |
| `Foo::{1}`       | stała o nazwie `'1'` (expr rzutowany na string)  |
| `Foo::{'BAR'}`   | to samo co `Foo::BAR`                            |

Cała przestrzeń **jedno-wyrażeniowa** `::{ expr }` jest zajęta. Literał kolekcji
jednoelementowej `UserIds::{1}` jest **nie do odróżnienia** od dynamicznego
dostępu do stałej. To jest dyskwalifikacja — dokładnie ten problem, o którym
mowa: *ten sam syntax definiowałby wartość przez zmienną*.

### Niuans (dla ścisłości)

Kolizja dotyczy **tylko** formy 1-elementowej (i pustej). Forma z przecinkiem
`UserIds::{1, 2, 3}` jest gramatycznie wolna i LALR(1)-czysta — po `::{expr`
lookahead `,` odróżnia kolekcję od `::{expr}` (dynamic const). Ale dawałoby to
absurd semantyczny:

```php
Foo::{1}       // dostęp do stałej '1'   (istniejąca funkcja)
Foo::{1, 2}    // kolekcja [1, 2]         (nowa funkcja)
```

Ta niespójność sama w sobie wystarcza, by odrzucić `::{}`. **`::{}` = odpada.**

---

## 2. Co NIE koliduje

Kolizja jest specyficzna dla klamer `{}`. Po `class_name ::` gramatyka
(`member_name`, `class_constant`, dostęp do property/metod) dopuszcza dziś
wyłącznie:

- `identifier`  (stała / metoda / `::class`)
- `{ expr }`    (dynamiczna nazwa — zajęte, patrz wyżej)
- `$zmienna`    (property statyczna / dynamiczna metoda)
- `class`       (`::class`)

Token **`[`** po `::` jest **nieużywany**. `Foo::[...]` jest dziś błędem
składniowym → delimiter jest wolny do wzięcia.

### Rekomendacja: `UserIds::[1, 2, 3]`

```php
$ids = UserIds::[1, 2, 3];        // literał kolekcji
$empty = UserIds::[];             // pusta kolekcja — też wolne
$one = UserIds::[42];             // jednoelementowa — brak kolizji
```

Zalety:

- **Zero kolizji** — `::[` nie występuje w żadnej istniejącej regule.
- **Spójność** — forma pusta, 1- i N-elementowa działają identycznie.
- **Wszędzie jako expression** — literał wpina się w `class_constant`, które
  jest w `scalar` → `expr` oraz w `fully_dereferenceable`. Czyli działa
  wszędzie gdzie expression, i jest dereferencyjny:
  `UserIds::[1, 2, 3][0]`, `UserIds::[1, 2, 3]->map(...)`.
- Wizualnie `[]` sugeruje kolekcję/tablicę — czytelne.

### Alternatywa bez zmian w składni

`UserIds::of(1, 2, 3)` lub `UserIds::from([1, 2, 3])` — statyczna metoda
wytwórcza. Działa dziś, zero zmian w silniku. Traci tylko „first-class" wygląd
literału.

---

## 3. Szkic implementacji `::[ ... ]` (gdyby wejść w PoC)

### Gramatyka (`Zend/zend_language_parser.y`, reguła `class_constant`)

```
| class_name          T_PAAMAYIM_NEKUDOTAYIM '[' array_pair_list ']'
      { $$ = zend_ast_create_class_collection($1, $4); }
| variable_class_name T_PAAMAYIM_NEKUDOTAYIM '[' array_pair_list ']'
      { $$ = zend_ast_create_class_collection($1, $4); }
```

`array_pair_list` daje za darmo: przecinek końcowy, listy, spread `...`,
formę pustą `[]`.

### Desugaring

Do wyboru — wynik to zwykły `ZEND_AST_STATIC_CALL` / `ZEND_AST_NEW`:

```
UserIds::[1, 2, 3]   ==>   UserIds::from([1, 2, 3])   // named constructor (rekom.)
                     lub   new UserIds([1, 2, 3])      // konstruktor
```

Konwencja `::from(array $items): static` pasuje do idiomu first-class
collection (prywatny konstruktor, walidacja, cache). Helper
`zend_ast_create_class_collection()` w `Zend/zend_ast.c` buduje węzeł
`STATIC_CALL(class, "from", ARG_LIST(ARRAY(...)))`.

### Uwaga o kontekstach stałych

Ponieważ węzeł trafia pod `class_constant` (a więc `scalar`), użycie w
kontekście const-expression (`const X = UserIds::[...]`, wartości domyślne)
da błąd „constant expression contains invalid operations" — to poprawne:
literał kolekcji jest wyrażeniem runtime, nie stałą. Zgodne z tym, jak
zachowuje się dziś dynamic const `Foo::{$x}`.

### Koszt buildu

Zmiana dotyczy **tylko** parsera (`bison`) — lexer bez zmian. Do pełnego
buildu php-src potrzebny jest jednak także `re2c` (generacja skanera z
`.l`); w tym środowisku `re2c` nie jest zainstalowany.

---

## 4. Wątki odłożone (kontekst z `brzuchal/php-collections`)

- **Aliasy** — odłożone: brak decyzji którą konwencję wybrać dla „pilnowania
  aliasu do `zval`". Nie dotknięte w tym spike'u.

---

## TL;DR

- `UserIds::{1, 2, 3}` → **odpada**: `::{ expr }` to już dynamiczny dostęp do
  stałej klasowej; forma 1-elementowa koliduje nieusuwalnie.
- `UserIds::[1, 2, 3]` → **wolne i wykonalne**, działa wszędzie jako
  expression, dereferencyjne, spójne dla 0/1/N elementów. Rekomendowany
  kierunek jeśli literał ma zostać.
- `UserIds::of(...)` / `::from([...])` → działa dziś bez zmian w składni.
