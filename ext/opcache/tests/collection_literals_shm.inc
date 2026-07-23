<?php
function ocl_flat(): int {
    $v = vec[int]{1, 2, 3};
    return zend_test_vec_count($v);
}

function ocl_nested(): int {
    $v = vec[vec[string]]{ vec[string]{'a'}, vec[string]{'b', 'c'} };
    return zend_test_vec_count(zend_test_vec_get($v, 1));
}

function ocl_empty(): int {
    return zend_test_vec_count(vec[int]{});
}

/* A multi-member tuple descriptor: exercises a persisted descriptor with more
 * than one member type, and the positional element check after unserialization. */
function ocl_tuple(): string {
    $t = tuple[int, string, vec[int]]{1, 'a', vec[int]{2, 3}};
    return zend_test_vec_get($t, 0) . ',' . zend_test_vec_get($t, 1)
        . ',' . zend_test_vec_count(zend_test_vec_get($t, 2));
}

function ocl_tuple_bad(): string {
    try {
        $t = tuple[int, string]{1, 2};
    } catch (TypeError $e) {
        return $e->getMessage();
    }
    return 'not rejected';
}

/* A persisted set descriptor, and its deduplication running after the script
 * has been unserialized from SHM. */
function ocl_set(): int {
    $s = set[int]{1, 2, 2, 3, 1};
    return zend_test_vec_count($s);
}

/* Returned so a caller can check that repeated executions of a *persisted*
 * literal site keep borrowing the same canonical node. */
function ocl_make(int $n) {
    return vec[int]{$n, $n + 1};
}

class OclC {
    /* Two literal sites in one op_array, plus a class-typed element type, so
     * more than one descriptor table entry has to survive persistence. */
    public function pair(): array {
        $a = vec[int]{10};
        $b = vec[OclC]{$this};
        return [zend_test_vec_get($a, 0), zend_test_vec_count($b)];
    }

    public function bad(): string {
        try {
            $v = vec[int]{1, 'x'};
        } catch (TypeError $e) {
            return $e->getMessage();
        }
        return 'not rejected';
    }
}

function ocl_report(): string {
    $c = new OclC();
    return implode('|', [
        ocl_flat(),
        ocl_nested(),
        ocl_empty(),
        implode(',', $c->pair()),
        $c->bad(),
        ocl_tuple(),
        ocl_tuple_bad(),
        ocl_set(),
    ]);
}
