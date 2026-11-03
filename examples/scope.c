int main() {
    int x = 10;
    {
        int x = 3;
        x = x + 2;
    }
    return x; // 10: внутрішній x не змінює зовнішній
}
