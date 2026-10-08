"""Reject sanitizer diagnostics on success paths, including the historical exit1 sentinel."""
import re

SANITIZER = re.compile(r'AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|MemorySanitizer|ThreadSanitizer|runtime error:', re.I)

def accepted(code, output, expected, required=None, *, expected_sanitizer=False):
    if code not in expected or (required and required not in output):
        return False
    if expected_sanitizer:
        # Only the explicit old UAR counterexample is allowed to contain errors.
        return 'stack-use-after-return' in output and 'AddressSanitizer' in output
    return not SANITIZER.search(output)

if __name__ == '__main__':
    sentinel='descriptor returned=OK length=2 pending=0 notification=0x00000000'
    assert accepted(1,sentinel,[1],sentinel)
    assert accepted(0,'CONTROL_PASS',[0],'CONTROL_PASS')
    for diagnostic in ['ERROR: AddressSanitizer: stack-use-after-return','SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior','x.cpp:3: runtime error: null pointer','ERROR: LeakSanitizer: detected memory leaks']:
        assert not accepted(1,sentinel+'\n'+diagnostic,[1],sentinel)
        assert not accepted(0,'CONTROL_PASS\n'+diagnostic,[0],'CONTROL_PASS')
    assert not accepted(1,'missing',[1],sentinel)
    assert not accepted(0,sentinel,[1],sentinel)
    assert accepted(-6,'ERROR: AddressSanitizer: stack-use-after-return',[-6],'stack-use-after-return',expected_sanitizer=True)
    assert not accepted(-6,'ERROR: AddressSanitizer: heap-buffer-overflow',[-6],'stack-use-after-return',expected_sanitizer=True)
    print('RUNNER_GATE_PASS: success-line plus exit1/exit0 plus sanitizer rejected; explicit old UAR retained')
