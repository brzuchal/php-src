--TEST--
tokenizer: T_COLLECTION_HEAD is emitted only for a head directly before '{'
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

echo "-- the head before '{' is one token, the '{' stays separate --\n";
foreach (['vec{1}', 'map{1}', 'set{1}', 'tuple{1}', 'shape{1}'] as $s) {
    echo seq("f($s);"), "\n";
}

echo "-- adjacency is required: a space or comment is a plain T_STRING --\n";
echo seq('f(vec {1});'), "\n";
echo seq('f(vec/*c*/{1});'), "\n";

echo "-- the same names are ordinary class/type names before a body brace --\n";
echo seq('class Vec{}'), "\n";
echo seq('class C extends Set{}'), "\n";
echo seq('function f(): Set{}'), "\n";

echo "-- the bracket head and member access are unchanged --\n";
echo seq('f(vec[int]{1});'), "\n";
echo seq('Foo::vec{0};'), "\n";
echo seq('$o->vec;'), "\n";
?>
--EXPECT--
-- the head before '{' is one token, the '{' stays separate --
T_STRING '(' T_COLLECTION_HEAD '{' T_LNUMBER '}' ')' ';'
T_STRING '(' T_COLLECTION_HEAD '{' T_LNUMBER '}' ')' ';'
T_STRING '(' T_COLLECTION_HEAD '{' T_LNUMBER '}' ')' ';'
T_STRING '(' T_COLLECTION_HEAD '{' T_LNUMBER '}' ')' ';'
T_STRING '(' T_COLLECTION_HEAD '{' T_LNUMBER '}' ')' ';'
-- adjacency is required: a space or comment is a plain T_STRING --
T_STRING '(' T_STRING '{' T_LNUMBER '}' ')' ';'
T_STRING '(' T_STRING T_COMMENT '{' T_LNUMBER '}' ')' ';'
-- the same names are ordinary class/type names before a body brace --
T_CLASS T_COLLECTION_HEAD '{' '}'
T_CLASS T_STRING T_EXTENDS T_COLLECTION_HEAD '{' '}'
T_FUNCTION T_STRING '(' ')' ':' T_COLLECTION_HEAD '{' '}'
-- the bracket head and member access are unchanged --
T_STRING '(' T_VEC_LBRACKET T_STRING ']' '{' T_LNUMBER '}' ')' ';'
T_STRING T_DOUBLE_COLON T_STRING '{' T_LNUMBER '}' ';'
T_VARIABLE T_OBJECT_OPERATOR T_STRING ';'
