
gen = ()
{
	-> "World";
}

run = (str)
{
	t = gen();
	sprintf(str, "Hello %s!\n", t);
	-> 0;
}

str = calloc(1, 255);
run(str);
printf(str);
free(str);

