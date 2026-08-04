--TEST--
collection types: references to typed properties verify collection values
--EXTENSIONS--
zend_test
--FILE--
<?php
zend_test_make_vec([1, 2], 'int', $vi);
zend_test_make_vec(['a'], 'string', $vs);

class C {
    public vec[int] $p;
    public mixed $m;
}
$c = new C();
$c->p = $vi;

/* A reference to a typed property carries a type source, so every write through
 * it is verified against vec[int]. */
$r = &$c->p;
try { $r = $vi; echo "vec[int] <- vec[int]: accepted\n"; }
catch (TypeError $e) { echo "vec[int] <- vec[int]: ", $e->getMessage(), "\n"; }
try { $r = $vs; echo "vec[int] <- vec[string]: accepted\n"; }
catch (TypeError $e) { echo "vec[int] <- vec[string]: rejected\n"; }
try { $r = 1; echo "vec[int] <- int: accepted\n"; }
catch (TypeError $e) { echo "vec[int] <- int: rejected\n"; }
unset($r);

$m = &$c->m;
try { $m = $vi; echo "mixed <- vec[int]: accepted\n"; }
catch (TypeError $e) { echo "mixed <- vec[int]: ", $e->getMessage(), "\n"; }
unset($m);

/* Ordinary references and referenced array buckets hold collections with plain
 * refcounted semantics: the reference aliases the slot, never the payload. */
$base = zend_test_refcount($vi);
$ref = &$vi;
var_dump(zend_test_refcount($vi) === $base);   // a reference does not copy the payload
$share = $vi;
var_dump(zend_test_refcount($vi) === $base + 1);
unset($share);
var_dump(zend_test_refcount($vi) === $base);
$ref = $vs;                                     // replacing the slot releases the old payload
var_dump(zend_test_refcount($vs) > 0);
unset($ref, $vi);

$arr = [];
$arr[0] = $vs;
$bucket = &$arr[0];
$after = zend_test_refcount($vs);
$bucket = $vs;                                  // write through a referenced bucket
var_dump(zend_test_refcount($vs) === $after);
unset($bucket, $arr);
var_dump(zend_test_refcount($vs) >= 1);
echo "done\n";
?>
--EXPECT--
vec[int] <- vec[int]: accepted
vec[int] <- vec[string]: rejected
vec[int] <- int: rejected
mixed <- vec[int]: accepted
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
