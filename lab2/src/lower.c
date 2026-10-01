        size_t read_length = 0;
        read(0, &read_length, sizeof(size_t));
        char *read_buffer = malloc(read_length + 1);
        read(0, &read_buffer, read_length);
        read_buffer[read_length] = '\0';