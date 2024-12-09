# Docker Swarm Deployment:

1. Initialize docker swarm
```bash
    $ docker swarm init
        or 
    $ docker swarm init --advertise-addr 192.168.1.7
```

2. Deploy the application
```bash
    $ docker stack deploy --compose-file docker-compose.yml detector-cpp-stack

    # If you want to add the exposed port then run: $ docker service update --publish-add 5000:5000 cpp-flask-app
```

3. Verify the deployment
```bash
    $ docker stack services detector-cpp-stack
    $ docker service ls
    $ docker service ps detector-cpp-stack_web
```

4. Clean up
```bash
    $ docker stack rm detector-cpp-stack
```
