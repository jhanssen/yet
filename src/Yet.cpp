#include "Yet.h"

void Yet::log(const char* fmt, ...)
{
    // code based on https://gist.github.com/EleotleCram/eb586037e2976a8d9884

    int i, j, len;
    bool cont = false;

    char buf[32];
    va_list argv;
    va_start(argv, fmt);
    len = 1;
    for(i = 0, j = 0; fmt[i] != '\0'; ++i, len = 1) {
        if (fmt[i] == '%') {
            Serial.write(reinterpret_cast<const uint8_t*>(fmt+j), i-j);

            do {
                cont = false;
                switch (fmt[++i]) {
                case 'l':
                    ++len;
                    cont = true;
                    break;
                case 'd':
                case 'i':
                    switch (len) {
                    case 1:
                        Serial.print(va_arg(argv, int));
                        break;
                    case 2:
                        Serial.print(va_arg(argv, long int));
                        break;
                    case 3:
                        snprintf(buf, sizeof(buf), "%lld", va_arg(argv, long long int));
                        Serial.print(buf);
                        break;
                    default:
                        Serial.print("too long int");
                        abort();
                    }
                    break;
                case 'u':
                    switch (len) {
                    case 1:
                        Serial.print(va_arg(argv, unsigned int));
                        break;
                    case 2:
                        Serial.print(va_arg(argv, unsigned long int));
                        break;
                    case 3:
                        snprintf(buf, sizeof(buf), "%llu", va_arg(argv, unsigned long long int));
                        Serial.print(buf);
                        break;
                    default:
                        Serial.print("too long int");
                        abort();
                    }
                    break;
                case 'x':
                    switch (len) {
                    case 1:
                        snprintf(buf, sizeof(buf), "%x", va_arg(argv, unsigned int));
                        break;
                    case 2:
                        snprintf(buf, sizeof(buf), "%lx", va_arg(argv, unsigned long int));
                        break;
                    case 3:
                        snprintf(buf, sizeof(buf), "%llx", va_arg(argv, unsigned long long int));
                        break;
                    default:
                        Serial.print("too long hex int");
                        abort();
                    }
                    Serial.write(buf);
                    break;
                case 'f':
                    switch (len) {
                    case 1:
                        Serial.print(va_arg(argv, float));
                        break;
                    case 2:
                        Serial.print(va_arg(argv, double));
                        break;
                    default:
                        Serial.print("too long float");
                        abort();
                    }
                    break;
                case 'c':
                    Serial.print((char) va_arg(argv, int));
                    break;
                case 's':
                    Serial.print(va_arg(argv, char *));
                    break;
                case 'p':
                    snprintf(buf, sizeof(buf), "%p", va_arg(argv, void*));
                    Serial.print(buf);
                    break;
                case '%':
                    Serial.print("%");
                    break;
                default:;
                }
            } while (cont);

            j = i+len;
        } else if (fmt[i] == '\n') {
            Serial.println("");
            j = i+1;
        }
    }
    va_end(argv);

    if(i > j) {
        Serial.write(reinterpret_cast<const uint8_t*>(fmt+j), i-j);
    }
}
