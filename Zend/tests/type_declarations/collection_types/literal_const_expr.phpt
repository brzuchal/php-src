--TEST--
collection literals: rejected in every constant-expression position (D-10)
--FILE--
<?php

/* A collection value borrows a type node owned by the request intern tier, so
 * it cannot be folded into a constant and persisted. Every constant-expression
 * position must reject it at compile time.
 *
 * A compile error is not catchable, so each position runs in its own process. */

$php = getenv('TEST_PHP_EXECUTABLE_ESCAPED');

$positions = [
    'constant'           => 'const C = vec[int]{1};',
    'class constant'     => 'class C1 { const X = vec[int]{1}; }',
    'parameter default'  => 'function f1($a = vec[int]{1}) {}',
    'property default'   => 'class C2 { public $p = vec[int]{1}; }',
    'enum case value'    => 'enum E: int { case A = vec[int]{1}; }',
    'attribute argument' => '#[Attr(vec[int]{1})] class C3 {}',
    'nested in an array' => 'const D = [1, vec[int]{2}];',
];

foreach ($positions as $label => $code) {
    $out = shell_exec($php . ' -n -r ' . escapeshellarg($code) . ' 2>&1');
    $line = trim(explode("\n", trim($out))[0]);
    echo $label, ': ', $line, "\n";
}

?>
--EXPECT--
constant: Fatal error: Collection literals are not allowed in constant expressions in Command line code on line 1
class constant: Fatal error: Collection literals are not allowed in constant expressions in Command line code on line 1
parameter default: Fatal error: Collection literals are not allowed in constant expressions in Command line code on line 1
property default: Fatal error: Collection literals are not allowed in constant expressions in Command line code on line 1
enum case value: Fatal error: Collection literals are not allowed in constant expressions in Command line code on line 1
attribute argument: Fatal error: Collection literals are not allowed in constant expressions in Command line code on line 1
nested in an array: Fatal error: Collection literals are not allowed in constant expressions in Command line code on line 1
