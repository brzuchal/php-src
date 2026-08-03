--TEST--
collection literals: the parser accepts all five heads, the compiler gates the unimplemented ones
--FILE--
<?php

/* The literal grammar is shared across the five heads and records only the
 * kind, so the kinds without a value representation are rejected by the same
 * compiler gate that rejects them in type positions -- not by a bare syntax
 * error, which would say nothing about why. */

$php = getenv('TEST_PHP_EXECUTABLE_ESCAPED');

$literals = [
    'vec'   => '$x = vec[int]{1};',
    'map'   => '$x = map[int, string]{1};',
    'set'   => '$x = set[int]{1};',
    'tuple' => "\$x = tuple[int, string]{1, 'a'};",  // single-quoted member: survives Windows cmd quoting
    'shape' => '$x = shape[int]{1};',
];

foreach ($literals as $head => $code) {
    $out = trim((string) shell_exec($php . ' -n -r ' . escapeshellarg($code) . ' 2>&1'));
    echo $head, ': ', $out === '' ? 'accepted' : trim(explode("\n", $out)[0]), "\n";
}

/* Adjacency is required, exactly as in type position. */
foreach (['$x = vec [int]{1};', '$x = vec/*c*/[int]{1};'] as $code) {
    $out = trim((string) shell_exec($php . ' -n -r ' . escapeshellarg($code) . ' 2>&1'));
    echo trim(explode("\n", $out)[0]), "\n";
}

?>
--EXPECT--
vec: accepted
map: Fatal error: Collection type map is not implemented yet in Command line code on line 1
set: accepted
tuple: accepted
shape: Fatal error: Collection type shape is not implemented yet in Command line code on line 1
Parse error: syntax error, unexpected token "{" in Command line code on line 1
Parse error: syntax error, unexpected token "{" in Command line code on line 1
