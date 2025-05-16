import pexpect
import os
import csv
import re
import subprocess

# Configuration
XV6_DIR = "./"  # Update this
RESULTS_CSV = "swap_efficiency.csv"
ALPHAS = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
BETAS = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]

def run_test(alpha, beta):
    """Run a single test with given alpha/beta values"""
    # Update Makefile with new alpha/beta values
    with open(os.path.join(XV6_DIR, "Makefile"), "r+") as f:
        content = f.read()
        content = re.sub(r'ALPHA \?= \d+', f'ALPHA ?= {alpha}', content)
        content = re.sub(r'BETA \?= \d+', f'BETA ?= {beta}', content)
        f.seek(0)
        f.write(content)
        f.truncate()

    # Rebuild and run test
    os.chdir(XV6_DIR)
    subprocess.run(["make", "clean"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)  # <--- Added make clean
    child = pexpect.spawn('make qemu-nox', timeout=30)
    
    test_results = {
        'alpha': alpha,
        'beta': beta,
        'ctrl_i_passed': False,
        'memtest_passed': False,
        'thresholds': []
    }

    try:
        # Wait for xv6 to boot
        child.expect('\$ ', timeout=20)
        
        # Test Ctrl+I
        child.sendcontrol('i')  # Send Ctrl+I
        child.expect('Ctrl\+I is detected by xv6', timeout=5)
        child.expect('PID NUM_PAGES', timeout=5)
        child.expect('1 3', timeout=5)  # Verify init process pages
        test_results['ctrl_i_passed'] = True

        # Run memtest
        child.sendline('memtest')
        
        # Capture threshold updates
        while True:
            index = child.expect([
                'Current Threshold = (\d+), Swapping (\d+) pages',
                'Memtest Passed',
                pexpect.TIMEOUT,
                pexpect.EOF
            ], timeout=15)
            
            if index == 0:
                threshold = int(child.match.group(1))
                pages = int(child.match.group(2))
                test_results['thresholds'].append((threshold, pages))
            elif index == 1:
                test_results['memtest_passed'] = True
                break
            else:
                break

    except pexpect.EOF:
        print("Unexpected EOF")
    except pexpect.TIMEOUT:
        print("Timeout occurred")
    finally:
        # Exit QEMU
        child.sendcontrol('a')
        child.send('x')
        child.close()
    
    return test_results

def main():
    with open(RESULTS_CSV, 'w', newline='') as csvfile:
        fieldnames = ['alpha', 'beta', 'ctrl_i', 'memtest', 'thresholds', 'swap_ops', 'total_pages']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        for alpha in ALPHAS:
            for beta in BETAS:
                print(f"Testing α={alpha}, β={beta}")
                results = run_test(alpha, beta)
                
                writer.writerow({
                    'alpha': alpha,
                    'beta': beta,
                    'ctrl_i': results['ctrl_i_passed'],
                    'memtest': results['memtest_passed'],
                    'thresholds': str(results['thresholds']),
                    'swap_ops': len(results['thresholds']),
                    'total_pages': sum(pages for _, pages in results['thresholds'])
                })

if __name__ == "__main__":
    main()