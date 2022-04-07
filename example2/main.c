int shared_var = 2;

int threadA(void)
{
    shared_var = 4;
    return 0;
}

int threadB(void)
{
    shared_var = 1;
    return 1;
}

int main(void)
{

    return 0;
}

