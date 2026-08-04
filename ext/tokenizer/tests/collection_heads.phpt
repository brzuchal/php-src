--TEST--
tokenizer: compound collection heads and the sequences that must not become one
--EXTENSIONS--
tokenizer
--FILE--
<?php
function seq(string $code): string {
    $out = [];
    foreach (token_get_all("<?php $code") as $t) {
        if (is_array($t)) {
            if ($t[0] === T_WHITESPACE || $t[0] === T_OPEN_TAG) continue;
            $out[] = token_name($t[0]);
        } else {
            $out[] = "'$t'";
        }
    }
    return implode(' ', $out);
}

echo "-- public constants --\n";
foreach (['T_VEC_LBRACKET', 'T_MAP_LBRACKET', 'T_SET_LBRACKET',
          'T_TUPLE_LBRACKET', 'T_SHAPE_LBRACKET'] as $c) {
    printf("%s %s\n", $c, token_name(constant($c)));
}

echo "-- a head adjacent to '[' is one token --\n";
foreach (['vec[int]', 'map[int,int]', 'set[int]', 'tuple[int]', 'shape[int]'] as $s) {
    echo seq("function f($s \$x){}"), "\n";
}

echo "-- trivia between name and '[' means no head --\n";
echo seq('f(vec [int]);'), "\n";
echo seq('f(vec/*c*/[int]);'), "\n";

echo "-- member access is never a head, with any trivia --\n";
echo seq('Foo::vec[0];'), "\n";
echo seq('Foo:: vec[0];'), "\n";
echo seq('Foo::/*c*/vec[0];'), "\n";
echo seq('$o->vec[0];'), "\n";
echo seq('$o?->vec[0];'), "\n";

echo "-- keywords after '::' keep their own tokens --\n";
echo seq('Foo::list;'), "\n";
echo seq('Foo::class;'), "\n";
?>
--EXPECT--
-- public constants --
T_VEC_LBRACKET T_VEC_LBRACKET
T_MAP_LBRACKET T_MAP_LBRACKET
T_SET_LBRACKET T_SET_LBRACKET
T_TUPLE_LBRACKET T_TUPLE_LBRACKET
T_SHAPE_LBRACKET T_SHAPE_LBRACKET
-- a head adjacent to '[' is one token --
T_FUNCTION T_STRING '(' T_VEC_LBRACKET T_STRING ']' T_VARIABLE ')' '{' '}'
T_FUNCTION T_STRING '(' T_MAP_LBRACKET T_STRING ',' T_STRING ']' T_VARIABLE ')' '{' '}'
T_FUNCTION T_STRING '(' T_SET_LBRACKET T_STRING ']' T_VARIABLE ')' '{' '}'
T_FUNCTION T_STRING '(' T_TUPLE_LBRACKET T_STRING ']' T_VARIABLE ')' '{' '}'
T_FUNCTION T_STRING '(' T_SHAPE_LBRACKET T_STRING ']' T_VARIABLE ')' '{' '}'
-- trivia between name and '[' means no head --
T_STRING '(' T_STRING '[' T_STRING ']' ')' ';'
T_STRING '(' T_STRING T_COMMENT '[' T_STRING ']' ')' ';'
-- member access is never a head, with any trivia --
T_STRING T_DOUBLE_COLON T_STRING '[' T_LNUMBER ']' ';'
T_STRING T_DOUBLE_COLON T_STRING '[' T_LNUMBER ']' ';'
T_STRING T_DOUBLE_COLON T_COMMENT T_STRING '[' T_LNUMBER ']' ';'
T_VARIABLE T_OBJECT_OPERATOR T_STRING '[' T_LNUMBER ']' ';'
T_VARIABLE T_NULLSAFE_OBJECT_OPERATOR T_STRING '[' T_LNUMBER ']' ';'
-- keywords after '::' keep their own tokens --
T_STRING T_DOUBLE_COLON T_LIST ';'
T_STRING T_DOUBLE_COLON T_CLASS ';'
